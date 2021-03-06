#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include "Configuration.h"
#include "Simulation.h"
#include "parallelManagers/PetscParallelConfiguration.h"
#include "MeshsizeFactory.h"
#include <iomanip>
#include "TurbulentSimulation.h"
#include "SimpleTimer.h"

int main (int argc, char *argv[]) {

    // Parallelization related. Initialize and identify
    // ---------------------------------------------------
    int rank;   // This processor's identifier
    int nproc;  // Number of processors in the group
    PetscInitialize(&argc, &argv, "petsc_commandline_arg", PETSC_NULL);
    MPI_Comm_size(PETSC_COMM_WORLD, &nproc);
    MPI_Comm_rank(PETSC_COMM_WORLD, &rank);
    #ifndef chkpt_to_vtk
    std::cout << "Rank: " << rank << ", Nproc: " << nproc << std::endl;
    #endif
    //----------------------------------------------------


    // read configuration and store information in parameters object
    Configuration configuration(argv[1]);
    Parameters parameters;
    configuration.loadParameters(parameters);
    #ifdef chkpt_to_vtk
    parameters.parallel.numProcessors[0] = 1;
    parameters.parallel.numProcessors[1] = 1;
    parameters.parallel.numProcessors[2] = 1;
    parameters.restart.startNew = false;
    #endif
    PetscParallelConfiguration parallelConfiguration(parameters);
    MeshsizeFactory::getInstance().initMeshsize(parameters);
    FlowField *flowField = NULL;
    Simulation *simulation = NULL;
    SimpleTimer timer = SimpleTimer();

    #ifndef chkpt_to_vtk
    #ifdef DEBUG
    std::cout << "Processor " << parameters.parallel.rank << " with index ";
    std::cout << parameters.parallel.indices[0] << ",";
    std::cout << parameters.parallel.indices[1] << ",";
    std::cout << parameters.parallel.indices[2];
    std::cout <<    " is computing the size of its subdomain and obtains ";
    std::cout << parameters.parallel.localSize[0] << ", ";
    std::cout << parameters.parallel.localSize[1] << " and ";
    std::cout << parameters.parallel.localSize[2];
    std::cout << ". Left neighbour: " << parameters.parallel.leftNb;
    std::cout << ", right neighbour: " << parameters.parallel.rightNb;
    std::cout << std::endl;
    std::cout << "Min. meshsizes: " << parameters.meshsize->getDxMin() << ", " << parameters.meshsize->getDyMin() << ", " << parameters.meshsize->getDzMin() << std::endl;
    std::cout << "Checkpoint iterations: " << parameters.checkpoint.iterations << ", directory: " << parameters.checkpoint.directory << ", prefix: " << parameters.checkpoint.prefix << ", cleanDirectory:" << parameters.checkpoint.cleanDirectory << std::endl;
    std::cout << "Restart filename: " << parameters.restart.filename << std::endl;
    #endif
    #endif

    // initialise simulation
    if (parameters.simulation.type=="turbulence"){
        #ifndef chkpt_to_vtk
        if(rank==0){ std::cout << "Start RANS simulation in " << parameters.geometry.dim << "D" << std::endl; }
        #endif
        // WS2: initialise turbulent flow field and turbulent simulation object
        TurbulentFlowField *turbFlowField = NULL;
        turbFlowField = new TurbulentFlowField(parameters);
        flowField = turbFlowField;
        if(flowField == NULL){ handleError(1, "flowField==NULL!"); }
        simulation = new TurbulentSimulation(parameters,*turbFlowField);
        // handleError(1,"Turbulence currently not supported yet!");
    } else if (parameters.simulation.type=="dns"){
        #ifndef chkpt_to_vtk
        if(rank==0){ std::cout << "Start DNS simulation in " << parameters.geometry.dim << "D" << std::endl; }
        #endif
        flowField = new FlowField(parameters);
        if(flowField == NULL){ handleError(1, "flowField==NULL!"); }
        simulation = new Simulation(parameters,*flowField);
    } else {
        handleError(1, "Unknown simulation type! Currently supported: dns, turbulence");
    }
    // call initialization of simulation (initialize flow field)
    if(simulation == NULL){ handleError(1, "simulation==NULL!"); }
    simulation->initializeFlowField();
    //flowField->getFlags().show();

    int timeSteps = 0;
    FLOAT time = 0.0;

    #ifdef chkpt_to_vtk
    // iterate through all checkpoint files
    DIR* dir_pointer = opendir(parameters.checkpoint.directory.c_str());
    dirent* file_pointer;
    while ((file_pointer = readdir(dir_pointer)) != NULL) {
        if (!strncmp(file_pointer->d_name, parameters.checkpoint.prefix.c_str(), parameters.checkpoint.prefix.size())) {
            parameters.restart.filename = parameters.checkpoint.directory + std::string(file_pointer->d_name);
            simulation->readCheckpoint(timeSteps, time);
            if (parameters.simulation.type=="turbulence") {
                ((TurbulentSimulation*)simulation)->computeTurbVisc();
            }
            std::cout << "Plotting time step " << timeSteps << std::endl;
            simulation->plotVTK(timeSteps);
        }
    }
    closedir(dir_pointer);

    // exit application
    delete simulation; simulation=NULL;
    delete flowField;  flowField= NULL;
    PetscFinalize();
    return 0;
    #endif

    // Read the restart data
    if(parameters.restart.filename != "") {
        simulation->readCheckpoint(timeSteps, time);
        if (parameters.restart.startNew) {
            timeSteps = 0;
            time = 0.0;
        }
    }

    FLOAT lastPlotTime = time;
    int lastCheckpointIter = timeSteps;
    FLOAT timeStdOut=parameters.stdOut.interval;

    // WS1: plot initial state
    if(parameters.restart.filename == "" && parameters.vtk.active) {
        simulation->plotVTK(timeSteps);
    }

    // initialize the region timers
    FLOAT time_loop  = 0; FLOAT time_loop_tot  = 0;
    FLOAT time_solve = 0; FLOAT time_solve_tot = 0;
    FLOAT time_comm  = 0; FLOAT time_comm_tot  = 0;

    // clean the checkpoints directory if needed
    if (parameters.checkpoint.cleanDirectory) {
        simulation->cleandirCheckpoint();
    }

    // start the global timer
    timer.start();

    // time loop
    while (time < parameters.simulation.finalTime){

        simulation->solveTimestep(time_solve, time_comm);

        time += parameters.timestep.dt;
        timeSteps++;

        // std-out: terminal info
        if ( (rank==0) && (timeStdOut <= time) ){
            std::cout << "Current time: " << time << "\ttimestep: " <<
                        parameters.timestep.dt << "\titeration: " << timeSteps <<std::endl << std::endl;
            timeStdOut += parameters.stdOut.interval;
        }

        // Create a checkpoint
        if (lastCheckpointIter + parameters.checkpoint.iterations <= timeSteps) {
            simulation->createCheckpoint(timeSteps, time);
            lastCheckpointIter += parameters.checkpoint.iterations;
            if (parameters.checkpoint.increaseIter) {
                if (parameters.checkpoint.maxIter > 0) {
                    parameters.checkpoint.iterations = std::min<int>(floor(parameters.checkpoint.incrFactor * parameters.checkpoint.iterations), parameters.checkpoint.maxIter);
                } else {
                    parameters.checkpoint.iterations = floor(parameters.checkpoint.incrFactor * parameters.checkpoint.iterations);
                }
                
            }
        }

        // WS1: trigger VTK output
        if (parameters.vtk.active && lastPlotTime + parameters.vtk.interval <= time) {
            simulation->plotVTK(timeSteps); // TODO Change to time?
            lastPlotTime += parameters.vtk.interval;
        }
    }

    // take computation time
    time_loop = timer.getTimeAndContinue();
    printf("[Rank %d] Timers (s):\tloop: %f | solve: %f  comm: %f  other: %f\n", rank, time_loop, time_solve, time_comm, time_loop-time_solve-time_comm);
    MPI_Reduce(&time_loop,  &time_loop_tot,  1, MY_MPI_FLOAT, MPI_SUM, 0, PETSC_COMM_WORLD);
    MPI_Reduce(&time_solve, &time_solve_tot, 1, MY_MPI_FLOAT, MPI_SUM, 0, PETSC_COMM_WORLD);
    MPI_Reduce(&time_comm,  &time_comm_tot,  1, MY_MPI_FLOAT, MPI_SUM, 0, PETSC_COMM_WORLD);
    if (rank==0) {
        printf("[Average] Timers (s):\tloop: %f | solve: %f  comm: %f  other: %f\n", time_loop_tot/nproc, time_solve_tot/nproc, time_comm_tot/nproc, (time_loop_tot-time_solve_tot-time_comm_tot)/nproc);
        std::cerr << parameters.parallel.numProcessors[0] << "x" << parameters.parallel.numProcessors[1] << "x" << parameters.parallel.numProcessors[2] << ": " << time_loop_tot/nproc << std::endl; // Output time in cerr for easy redirection into file
    }

    // Create the final checkpoint
    simulation->createCheckpoint(timeSteps, time);

    // WS1: plot final output
    if(parameters.vtk.active) {
        simulation->plotVTK(timeSteps);
    }

    delete simulation; simulation=NULL;
    delete flowField;  flowField= NULL;

    PetscFinalize();
}
