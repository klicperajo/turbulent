#ifndef _MESHSIZEFACTORY_H_
#define _MESHSIZEFACTORY_H_

#include "Definitions.h"
#include "Parameters.h"


/** initialises the meshsize in the Parameters. Must be called after configuring (Configuration and PetscParallelConfiguration).
 *  We therefore make use of the singleton/factory pattern.
 *  @author Philipp Neumann
 */
class MeshsizeFactory {
  public:
    static MeshsizeFactory& getInstance(){
      static MeshsizeFactory singleton;
      return singleton;
    }

    void initMeshsize(Parameters& parameters){
      // initialise meshsize
      switch (parameters.geometry.meshsizeType){
        // uniform meshsize
        case Uniform:
          parameters.meshsize = new UniformMeshsize(parameters);
          break;
        // tanh-stretched mesh
        case TanhStretching:{
          TanhMeshStretching * mesh = new TanhMeshStretching(
                                  parameters,
                                  (bool)parameters.geometry.stretchX,(bool)parameters.geometry.stretchY,(bool)parameters.geometry.stretchZ
                                  // ,parameters.geometry.deltaSX,parameters.geometry.deltaSY,parameters.geometry.deltaSZ
                                );
          mesh->precomputeCoordinates();
          parameters.meshsize = mesh;
          // parameters.meshsize = new TanhMeshStretching(
          //                         parameters,
          //                         (bool)parameters.geometry.stretchX,(bool)parameters.geometry.stretchY,(bool)parameters.geometry.stretchZ
          //                       );
          break;}
        // stretched mesh for bfs
        case BfsStretching:{
          BfsMeshStretching * mesh = new BfsMeshStretching(parameters
                                  // ,parameters.geometry.deltaSX,parameters.geometry.deltaSY,parameters.geometry.deltaSZ
                                );
          mesh->precomputeCoordinates();
          parameters.meshsize = mesh;
          // parameters.meshsize = new BfsMeshStretching(
          //                         parameters
          //                       );
          break;}
        default:
          handleError(1,"Unknown meshsize type!");
          break;
      }
      // check that meshsize is initialised
      if (parameters.meshsize==NULL){handleError(1,"parameters.meshsize==NULL!"); }
    }

  private:
    MeshsizeFactory(){}
    ~MeshsizeFactory(){}
};


#endif // _MESHSIZEFACTORY_H_
