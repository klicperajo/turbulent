#include "Configuration.h"
#include "3rdparty/tinyxml2/tinyxml2.h"
#include <string>
#include <sstream>
#include <dirent.h>
#include "Parameters.h"

void readFloatMandatory(FLOAT & storage, tinyxml2::XMLElement *node, const char* tag){
    double value;   // Use to be able to select precision
    if (node->QueryDoubleAttribute(tag, &value) != tinyxml2::XML_NO_ERROR){
        handleError(1, "Error while reading mandatory argument");
    } else {
        storage = (FLOAT)value;
    }
}

void readFloatOptional(FLOAT & storage, tinyxml2::XMLElement *node, const char* tag,
                       FLOAT defaultValue = 0){
    double value;   // Use to be able to select precision
    int result = node->QueryDoubleAttribute(tag, &value);
    if (result == tinyxml2::XML_NO_ATTRIBUTE){
        storage = defaultValue;
    } else if (result == tinyxml2::XML_WRONG_ATTRIBUTE_TYPE){
        handleError(1, "Error while reading optional argument");
    } else {
        storage = (FLOAT)value;
    }
}

void readIntMandatory(int & storage, tinyxml2::XMLElement *node, const char* tag){
    int value;
    if (node->QueryIntAttribute(tag, &value) != tinyxml2::XML_NO_ERROR){
        handleError(1, "Error while reading mandatory argument");
    } else {
        storage = value;
    }
}

void readIntOptional(int & storage, tinyxml2::XMLElement *node, const char* tag,
                       int defaultValue = 0){
    int result = node->QueryIntAttribute(tag, &storage);
    if (result == tinyxml2::XML_NO_ATTRIBUTE){
        storage = defaultValue;
    } else if (result == tinyxml2::XML_WRONG_ATTRIBUTE_TYPE){
        handleError(1, "Error while reading optional argument");
    }
}


void readBoolMandatory(bool &storage, tinyxml2::XMLElement *node, const char *tag){
  bool value;
  if (node->QueryBoolAttribute(tag, &value) != tinyxml2::XML_NO_ERROR){
      handleError(1, "Error while reading mandatory argument");
  } else {
      storage = value;
  }
}


void readBoolOptional(bool & storage, tinyxml2::XMLElement *node, const char* tag,
                      bool defaultValue = false){
    int result = node->QueryBoolAttribute(tag, &storage);
    if (result == tinyxml2::XML_NO_ATTRIBUTE){
        storage = defaultValue;
    } else if (result == tinyxml2::XML_WRONG_ATTRIBUTE_TYPE){
        handleError(1, "Error while reading optional argument");
    }
}

void readStringMandatory(std::string & storage, tinyxml2::XMLElement *node){
    const char *myText = node->GetText();
    if (myText == NULL){
        const std::string nodename = node->Name();
        std::cerr << "ERROR in file " << __FILE__ << ", line " << __LINE__ << ": ";
        std::cerr << "No string specified for this node: " << nodename << std::endl;
        exit(2);
    } else {
      storage = node->GetText();
      if (!storage.compare("")){
        handleError(1, "Missing mandatory string!");
      }
    }
}

void readWall(tinyxml2::XMLElement *wall, FLOAT *vector, FLOAT &scalar){
    tinyxml2::XMLElement *quantity = wall->FirstChildElement("vector");
    if (quantity != NULL){
        readFloatOptional(vector[0], quantity, "x");
        readFloatOptional(vector[1], quantity, "y");
        readFloatOptional(vector[2], quantity, "z");
    }
    quantity = wall->FirstChildElement("scalar");
    if (quantity != NULL){
        readFloatOptional(scalar, quantity, "value");
    }
}

void broadcastString (std::string & target, const MPI_Comm & communicator, int root = 0){
    int stringSize, rank;
    MPI_Comm_rank(communicator, &rank);
    if (rank == root){
        stringSize = target.size();
    }
    MPI_Bcast(&stringSize, 1, MPI_INT, 0, communicator);
    char *name = new char[stringSize + 1];    // One more for the null character
    if (rank == root) {
        target.copy(name, stringSize, 0);
    }
    name[stringSize] = '\0';
    MPI_Bcast(name, stringSize+1, MPI_CHAR, 0, communicator);
    if (rank != root){
        target = name;
    }
    delete [] name; name = NULL;
}

Configuration::Configuration(){
    _filename = "";
}

Configuration::Configuration(const std::string & filename){
    _filename = filename;
}

void Configuration::setFileName(const std::string & filename){
    _filename = filename;
}

void Configuration::loadParameters(Parameters & parameters, const MPI_Comm & communicator){
    tinyxml2::XMLDocument confFile;
    tinyxml2::XMLElement *node;
    tinyxml2::XMLElement *subNode;


    int rank;

    MPI_Comm_rank(communicator, &rank);

    // we only read on rank 0; afterwards, all configuration parameters are broadcasted to all processes.
    // So, if you add new parameters in the configuration, make sure to broadcast them to the other processes!
    if (rank == 0){

        // Parse the configuration file and check validity
        confFile.LoadFile(_filename.c_str());
        if (confFile.FirstChildElement() == NULL){
            handleError(1, "Error parsing the configuration file");
        }

        //--------------------------------------------------
        // Load geometric parameters
        //--------------------------------------------------
        node = confFile.FirstChildElement()->FirstChildElement("geometry");

        if (node == NULL){
            handleError(1, "Error loading geometry properties");
        }

        readIntMandatory(parameters.geometry.sizeX, node, "sizeX");
        readIntMandatory(parameters.geometry.sizeY, node, "sizeY");
        readIntOptional (parameters.geometry.sizeZ, node, "sizeZ");

        if (parameters.geometry.sizeX < 2 || parameters.geometry.sizeY < 2 ||
                parameters.geometry.sizeZ < 0){
            handleError(1, "Invalid size specified in configuration file");
        }

        parameters.geometry.dim = 0;
        if (node->QueryIntAttribute("dim", &(parameters.geometry.dim)) !=
                tinyxml2::XML_WRONG_ATTRIBUTE_TYPE){
            if (parameters.geometry.dim == 0){
                if (parameters.geometry.sizeZ == 0){
                    parameters.geometry.sizeZ = 1;
                    parameters.geometry.dim = 2;
                } else {
                    parameters.geometry.dim = 3;
                }
            }
        }

        if (parameters.geometry.dim == 3 && parameters.geometry.sizeZ == 1){
            handleError(1, "Inconsistent data: 3D geometry specified with Z size zero");
        }

        // Determine the sizes of the cells

        readFloatMandatory (parameters.geometry.lengthX, node, "lengthX");
        readFloatMandatory (parameters.geometry.lengthY, node, "lengthY");
        readFloatMandatory (parameters.geometry.lengthZ, node, "lengthZ");
        // read geometry->meshsize parameters
        std::string meshsizeType="";
        subNode = node->FirstChildElement("mesh");
        readStringMandatory(meshsizeType,subNode);
        if (meshsizeType == "uniform"){
          parameters.geometry.meshsizeType = Uniform;
        } else if (meshsizeType == "stretched"){
          parameters.geometry.meshsizeType = TanhStretching;
          bool buffer=false;
          readBoolMandatory(buffer, node,"stretchX");
          parameters.geometry.stretchX = (int) buffer;
          readBoolMandatory(buffer, node,"stretchY");
          parameters.geometry.stretchY = (int) buffer;
          if (parameters.geometry.dim == 3){
            readBoolMandatory(buffer, node,"stretchZ");
            parameters.geometry.stretchZ = (int) buffer;
          } else {
            parameters.geometry.stretchZ = false;
          }
          readFloatOptional(parameters.geometry.deltaSX,subNode,"deltaSX",parameters.geometry.deltaSX);
          readFloatOptional(parameters.geometry.deltaSY,subNode,"deltaSY",parameters.geometry.deltaSY);
          readFloatOptional(parameters.geometry.deltaSZ,subNode,"deltaSZ",parameters.geometry.deltaSZ);
        } else if (meshsizeType == "bfs"){
          parameters.geometry.meshsizeType = BfsStretching;
          readFloatOptional(parameters.geometry.deltaSX,subNode,"deltaSX",parameters.geometry.deltaSX);
          readFloatOptional(parameters.geometry.deltaSY,subNode,"deltaSY",parameters.geometry.deltaSY);
          readFloatOptional(parameters.geometry.deltaSZ,subNode,"deltaSZ",parameters.geometry.deltaSZ);
        } else {
          handleError(1, "Unknown 'mesh'!");
        }

        // Now, the size of the elements should be set

        _dim = parameters.geometry.dim;

        //--------------------------------------------------
        // Timestep parameters
        //--------------------------------------------------

        node = confFile.FirstChildElement()->FirstChildElement("timestep");

        if (node == NULL){
            handleError(1, "Error loading timestep parameters");
        }

        readFloatOptional(parameters.timestep.dt, node, "dt", 1);
        readFloatOptional(parameters.timestep.tau, node, "tau", 0.5);

        //--------------------------------------------------
        // Flow parameters
        //--------------------------------------------------

        node = confFile.FirstChildElement()->FirstChildElement("flow");

        if (node == NULL){
            handleError(1, "Error loading flow parameters");
        }

        readFloatMandatory(parameters.flow.Re, node, "Re");

        //--------------------------------------------------
        // Solver parameters
        //--------------------------------------------------

        node = confFile.FirstChildElement()->FirstChildElement("solver");

        if (node == NULL){
            handleError(1, "Error loading solver parameters");
        }

        readFloatMandatory(parameters.solver.gamma, node, "gamma");
        readIntOptional (parameters.solver.maxIterations, node, "maxIterations");

        //--------------------------------------------------
        // Environmental parameters
        //--------------------------------------------------

        node = confFile.FirstChildElement()->FirstChildElement("environment");

        if (node == NULL){
            handleError(1, "Error loading environmental  parameters");
        }

        readFloatOptional(parameters.environment.gx, node, "gx");
        readFloatOptional(parameters.environment.gy, node, "gy");
        readFloatOptional(parameters.environment.gz, node, "gz");

        //--------------------------------------------------
        // Simulation parameters
        //--------------------------------------------------

        node = confFile.FirstChildElement()->FirstChildElement("simulation");

        if (node == NULL){
            handleError(1, "Error loading simulation parameters");
        }

        readFloatMandatory(parameters.simulation.finalTime, node, "finalTime");

        subNode = node->FirstChildElement("type");
        if (subNode != NULL){
            readStringMandatory(parameters.simulation.type, subNode);
        } else {
            handleError (1, "Missing type in simulation parameters");
        }

        subNode = node->FirstChildElement("scenario");
        if (subNode != NULL){
            readStringMandatory(parameters.simulation.scenario, subNode);
        } else {
            handleError (1, "Missing scenario in simulation parameters");
        }

        //--------------------------------------------------
        // VTK parameters
        //--------------------------------------------------

        node = confFile.FirstChildElement()->FirstChildElement("vtk");

        bool buffer = true;
        if (node == NULL) {
            parameters.vtk.active = (int) false;
        } else {
            readBoolOptional(buffer, node, "active", true);
            parameters.vtk.active = (int) buffer;
            readFloatOptional(parameters.vtk.interval, node, "interval");
            readStringMandatory(parameters.vtk.prefix, node);
        }

        //--------------------------------------------------
        // StdOut parameters
        //--------------------------------------------------

        node = confFile.FirstChildElement()->FirstChildElement("stdOut");

        if (node == NULL){
            handleError(1, "Error loading StdOut parameters");
        }

        // If no value given, print every step
        readFloatOptional(parameters.stdOut.interval, node, "interval", 1);

        //--------------------------------------------------
        // Checkpoint parameters
        //--------------------------------------------------
        node = confFile.FirstChildElement()->FirstChildElement("checkpoint");

        if (node == NULL){
            handleError(1, "Error loading checkpointing parameters");
        }

        readIntOptional(parameters.checkpoint.iterations, node, "iterations", 1000);
        readBoolOptional(buffer, node, "increaseIter", false);
        parameters.checkpoint.increaseIter = (int) buffer;
        readIntOptional(parameters.checkpoint.maxIter, node, "maxIter", -1);
        readFloatOptional(parameters.checkpoint.incrFactor, node, "incrFactor", 1.1);
        readBoolOptional(buffer, node, "cleanDirectory", false);
        parameters.checkpoint.cleanDirectory = (int) buffer;

        subNode = node->FirstChildElement("directory");
        if (subNode != NULL) {
            readStringMandatory(parameters.checkpoint.directory, subNode);
        } else {
            handleError (1, "Missing directory in checkpoint parameters");
        }

        subNode = node->FirstChildElement("prefix");
        if (subNode != NULL) {
            readStringMandatory(parameters.checkpoint.prefix, subNode);
        } else {
            handleError (1, "Missing prefix in checkpoint parameters");
        }

        //--------------------------------------------------
        // Restart parameters
        //--------------------------------------------------
        node = confFile.FirstChildElement()->FirstChildElement("restart");

        if (node != NULL){
            readStringMandatory(parameters.restart.filename, node);

            readBoolOptional(buffer, node, "latest", false);
            parameters.restart.latest = (int) buffer;

            // Change filename to the latest file
            if(parameters.restart.latest) {

                // get the prefix
                std::string restart_prefix = parameters.restart.filename;
                size_t prefix_end = parameters.restart.filename.find_last_of(".");
                if (prefix_end != std::string::npos) {
                    restart_prefix = parameters.restart.filename.substr(0,prefix_end);
                }

                // get the directory
                size_t dir_end = parameters.restart.filename.find_last_of("/");
                std::stringstream restart_dir;
                restart_dir << ".";
                if (dir_end != std::string::npos) {
                    restart_dir << "/" << parameters.restart.filename.substr(0, dir_end);
                    restart_prefix = restart_prefix.substr(dir_end + 1);
                    parameters.restart.filename = parameters.restart.filename.substr(dir_end + 1);
                }

                // get the latest checkpoint file
                DIR* dir_pointer = opendir(restart_dir.str().c_str());
                dirent* file_pointer;
                while ((file_pointer = readdir(dir_pointer)) != NULL) {
                    if ( !strncmp(file_pointer->d_name, restart_prefix.c_str(), restart_prefix.size()) &&
                        strcmp(file_pointer->d_name, parameters.restart.filename.c_str()) > 0) {
                            parameters.restart.filename = std::string(file_pointer->d_name);
                    }
                }
                closedir(dir_pointer);

                // readd the directory to the filename
                std::stringstream tmp_fstream;
                tmp_fstream << restart_dir.str() << "/" << parameters.restart.filename;
                parameters.restart.filename = tmp_fstream.str();
            }

            readBoolOptional(buffer, node, "startNew", false);
            parameters.restart.startNew = (int) buffer;
        }

        //--------------------------------------------------
        // Parallel parameters
        //--------------------------------------------------

        node = confFile.FirstChildElement()->FirstChildElement("parallel");

        if (node == NULL){
            handleError(1, "Error loading parallel parameters");
        }

        readIntOptional(parameters.parallel.numProcessors[0], node, "numProcessorsX", 1);
        readIntOptional(parameters.parallel.numProcessors[1], node, "numProcessorsY", 1);
        readIntOptional(parameters.parallel.numProcessors[2], node, "numProcessorsZ", 1);

        // Start neighbors on null in case that no parallel configuration is used later.
        parameters.parallel.leftNb = MPI_PROC_NULL;
        parameters.parallel.rightNb = MPI_PROC_NULL;
        parameters.parallel.bottomNb = MPI_PROC_NULL;
        parameters.parallel.topNb = MPI_PROC_NULL;
        parameters.parallel.frontNb = MPI_PROC_NULL;
        parameters.parallel.backNb = MPI_PROC_NULL;

        // Yet more parameters initialized in case that no parallel configuration is applied
        parameters.parallel.localSize[0] = parameters.geometry.sizeX;
        parameters.parallel.localSize[1] = parameters.geometry.sizeY;
        parameters.parallel.localSize[2] = parameters.geometry.sizeZ;

        parameters.parallel.firstCorner[0] = 0;
        parameters.parallel.firstCorner[1] = 0;
        parameters.parallel.firstCorner[2] = 0;

        // VTK output is named after the rank, so we define it here, again, in case that it's not
        // initialized anywhere else.
        parameters.parallel.rank = rank;

        //--------------------------------------------------
        // Walls
        //--------------------------------------------------

        node = confFile.FirstChildElement()->FirstChildElement("walls");

        if (node == NULL){
            handleError(1, "Error loading wall parameters");
        }


        tinyxml2::XMLElement *wall;
        wall = node->FirstChildElement("left");
        if (wall != NULL){
            readWall(wall, parameters.walls.vectorLeft, parameters.walls.scalarLeft);
        }

        wall = node->FirstChildElement("right");
        if (wall != NULL){
            readWall(wall, parameters.walls.vectorRight, parameters.walls.scalarRight);
        }

        wall = node->FirstChildElement("bottom");
        if (wall != NULL){
            readWall(wall, parameters.walls.vectorBottom, parameters.walls.scalarBottom);
        }

        wall = node->FirstChildElement("top");
        if (wall != NULL){
            readWall(wall, parameters.walls.vectorTop, parameters.walls.scalarTop);
        }

        wall = node->FirstChildElement("front");
        if (wall != NULL){
            readWall(wall, parameters.walls.vectorFront, parameters.walls.scalarFront);
        }

        wall = node->FirstChildElement("back");
        if (wall != NULL){
            readWall(wall, parameters.walls.vectorBack, parameters.walls.scalarBack);
        }

        // Set the scalar values to zero;
        // do not set the left pressure value to zero, if we have a pressure-channel
        // scenario -> in this case, we need a fixed pressure value there
        if (parameters.simulation.scenario != "pressure-channel" ){
          parameters.walls.scalarLeft   = 0.0;
        }
        parameters.walls.scalarRight  = 0.0;
        parameters.walls.scalarBottom = 0.0;
        parameters.walls.scalarTop    = 0.0;
        parameters.walls.scalarFront  = 0.0;
        parameters.walls.scalarBack   = 0.0;


        //--------------------------------------------------
        // Backward facing step
        //--------------------------------------------------
        parameters.bfStep.xRatio = -1.0;
        parameters.bfStep.yRatio = -1.0;
        node = confFile.FirstChildElement()->FirstChildElement("backwardFacingStep");
        if (node != NULL){
            readFloatMandatory(parameters.bfStep.xRatio, node, "xRatio");
            readFloatMandatory(parameters.bfStep.yRatio, node, "yRatio");
        }


        //------------------------------------------------------
        // WS2: Turbulence model
        //------------------------------------------------------
        node = confFile.FirstChildElement()->FirstChildElement("turbulenceModel");
        if (node != NULL) {
            subNode = node->FirstChildElement("type");
            readStringMandatory(parameters.turbulenceModel.type, subNode);
            subNode = node->FirstChildElement("mixingLengthModel");
            if (subNode != NULL) {
                readFloatOptional(parameters.turbulenceModel.mixingLengthModel.kappa,
                                  subNode, "kappa", parameters.turbulenceModel.mixingLengthModel.kappa);
                subNode = subNode->FirstChildElement("delta");
                if (subNode != NULL) {
                    std::string deltaType;
                    readStringMandatory(deltaType, subNode);
                    if (deltaType == "ignored"){
                        parameters.turbulenceModel.mixingLengthModel.deltaType = IgnoreDelta;
                    }else if (deltaType == "fixed"){
                        parameters.turbulenceModel.mixingLengthModel.deltaType = FixedDelta;
                        readFloatMandatory(parameters.turbulenceModel.mixingLengthModel.deltaValue,
                                          subNode, "fixedValue");
                    }else if (deltaType == "Blasius"){
                        parameters.turbulenceModel.mixingLengthModel.deltaType = BlasiusLayer;
                    }else if (deltaType == "turbulentFlatPlate"){
                        parameters.turbulenceModel.mixingLengthModel.deltaType = TurbulentFlatPlate;
                    }else{
                      handleError(1, "Error loading mixing length model parameters (delta)")
                    }
                }
                // TODO: revise mixing length model parameters
            }
        }
    }

    // Broadcasting of the values
    MPI_Bcast(&(parameters.geometry.sizeX), 1, MPI_INT, 0, communicator);
    MPI_Bcast(&(parameters.geometry.sizeY), 1, MPI_INT, 0, communicator);
    MPI_Bcast(&(parameters.geometry.sizeZ), 1, MPI_INT, 0, communicator);

    MPI_Bcast(&(parameters.geometry.dim), 1, MPI_INT, 0, communicator);

    MPI_Bcast(&(parameters.geometry.meshsizeType), 1, MPI_INT, 0, communicator);
    MPI_Bcast(&(parameters.geometry.deltaSX), 1, MY_MPI_FLOAT, 0, communicator);
    MPI_Bcast(&(parameters.geometry.deltaSY), 1, MY_MPI_FLOAT, 0, communicator);
    MPI_Bcast(&(parameters.geometry.deltaSZ), 1, MY_MPI_FLOAT, 0, communicator);
    MPI_Bcast(&(parameters.geometry.stretchX),1,MPI_INT,0,communicator);
    MPI_Bcast(&(parameters.geometry.stretchY),1,MPI_INT,0,communicator);
    MPI_Bcast(&(parameters.geometry.stretchZ),1,MPI_INT,0,communicator);
    MPI_Bcast(&(parameters.geometry.lengthX),  1, MY_MPI_FLOAT, 0, communicator);
    MPI_Bcast(&(parameters.geometry.lengthY),  1, MY_MPI_FLOAT, 0, communicator);
    MPI_Bcast(&(parameters.geometry.lengthZ),  1, MY_MPI_FLOAT, 0, communicator);

    MPI_Bcast(&(parameters.timestep.dt),  1, MY_MPI_FLOAT, 0, communicator);
    MPI_Bcast(&(parameters.timestep.tau), 1, MY_MPI_FLOAT, 0, communicator);

    MPI_Bcast(&(parameters.flow.Re), 1, MY_MPI_FLOAT, 0, communicator);

    MPI_Bcast(&(parameters.solver.gamma),         1, MY_MPI_FLOAT, 0, communicator);
    MPI_Bcast(&(parameters.solver.maxIterations), 1, MY_MPI_FLOAT, 0, communicator);

    MPI_Bcast(&(parameters.environment.gx), 1, MY_MPI_FLOAT, 0, communicator);
    MPI_Bcast(&(parameters.environment.gy), 1, MY_MPI_FLOAT, 0, communicator);
    MPI_Bcast(&(parameters.environment.gz), 1, MY_MPI_FLOAT, 0, communicator);

    MPI_Bcast(&(parameters.simulation.finalTime), 1, MY_MPI_FLOAT, 0, communicator);

    MPI_Bcast(&(parameters.vtk.active), 1, MPI_INT, 0, communicator);
    MPI_Bcast(&(parameters.vtk.interval), 1, MY_MPI_FLOAT, 0, communicator);
    MPI_Bcast(&(parameters.stdOut.interval), 1, MY_MPI_FLOAT, 0, communicator);
    MPI_Bcast(&(parameters.checkpoint.iterations), 1, MPI_INT, 0, communicator);
    MPI_Bcast(&(parameters.checkpoint.maxIter), 1, MPI_INT, 0, communicator);
    MPI_Bcast(&(parameters.checkpoint.incrFactor), 1, MY_MPI_FLOAT, 0, communicator);

    broadcastString (parameters.vtk.prefix, communicator);
    broadcastString (parameters.checkpoint.directory, communicator);
    broadcastString (parameters.checkpoint.prefix, communicator);
    broadcastString (parameters.restart.filename, communicator);
    broadcastString (parameters.simulation.type, communicator);
    broadcastString (parameters.simulation.scenario, communicator);

    MPI_Bcast(&(parameters.checkpoint.increaseIter),1,MPI_INT,0,communicator);
    MPI_Bcast(&(parameters.checkpoint.cleanDirectory),1,MPI_INT,0,communicator);
    MPI_Bcast(&(parameters.restart.latest),1,MPI_INT,0,communicator);
    MPI_Bcast(&(parameters.restart.startNew),1,MPI_INT,0,communicator);

    MPI_Bcast(&(parameters.bfStep.xRatio), 1, MY_MPI_FLOAT, 0, communicator);
    MPI_Bcast(&(parameters.bfStep.yRatio), 1, MY_MPI_FLOAT, 0, communicator);

    MPI_Bcast(parameters.parallel.numProcessors, 3, MPI_INT, 0, communicator);

    MPI_Bcast(&(parameters.walls.scalarLeft),   1, MY_MPI_FLOAT, 0, communicator);
    MPI_Bcast(&(parameters.walls.scalarRight),  1, MY_MPI_FLOAT, 0, communicator);
    MPI_Bcast(&(parameters.walls.scalarBottom), 1, MY_MPI_FLOAT, 0, communicator);
    MPI_Bcast(&(parameters.walls.scalarTop),    1, MY_MPI_FLOAT, 0, communicator);
    MPI_Bcast(&(parameters.walls.scalarFront),  1, MY_MPI_FLOAT, 0, communicator);
    MPI_Bcast(&(parameters.walls.scalarBack),   1, MY_MPI_FLOAT, 0, communicator);

    MPI_Bcast(parameters.walls.vectorLeft,   3, MY_MPI_FLOAT, 0, communicator);
    MPI_Bcast(parameters.walls.vectorRight,  3, MY_MPI_FLOAT, 0, communicator);
    MPI_Bcast(parameters.walls.vectorBottom, 3, MY_MPI_FLOAT, 0, communicator);
    MPI_Bcast(parameters.walls.vectorTop,    3, MY_MPI_FLOAT, 0, communicator);
    MPI_Bcast(parameters.walls.vectorFront,  3, MY_MPI_FLOAT, 0, communicator);
    MPI_Bcast(parameters.walls.vectorBack,   3, MY_MPI_FLOAT, 0, communicator);

    // WS2: broadcast turbulence parameters
    broadcastString (parameters.turbulenceModel.type, communicator);
    MPI_Bcast(&(parameters.turbulenceModel.mixingLengthModel.deltaType),  1, MPI_INT,      0, communicator);
    MPI_Bcast(&(parameters.turbulenceModel.mixingLengthModel.deltaValue), 1, MY_MPI_FLOAT, 0, communicator);
    MPI_Bcast(&(parameters.turbulenceModel.mixingLengthModel.kappa),      1, MY_MPI_FLOAT, 0, communicator);

}
