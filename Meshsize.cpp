#include "Meshsize.h"

#include "Parameters.h"

UniformMeshsize::UniformMeshsize(
  const Parameters &parameters
): Meshsize(),
_dx(parameters.geometry.lengthX/parameters.geometry.sizeX),
_dy(parameters.geometry.lengthY/parameters.geometry.sizeY),
_dz(parameters.geometry.dim==3 ? parameters.geometry.lengthZ/parameters.geometry.sizeZ : 0.0),
_firstCornerX(parameters.parallel.firstCorner[0]),
_firstCornerY(parameters.parallel.firstCorner[1]),
_firstCornerZ(parameters.geometry.dim==3 ? parameters.parallel.firstCorner[2] : 0)
{
  if (_dx<= 0.0){ handleError(1,"_dx<=0.0!"); }
  if (_dy<= 0.0){ handleError(1,"_dy<=0.0!"); }
  if (parameters.geometry.dim==3){
    if (_dz<=0.0){ handleError(1,"_dz<=0.0!"); }
  }
}

UniformMeshsize::~UniformMeshsize(){}



TanhMeshStretching::TanhMeshStretching(
  const Parameters & parameters,bool stretchX, bool stretchY, bool stretchZ, const FLOAT deltaS
): Meshsize(), _parameters(parameters), _uniformMeshsize(parameters),
   _lengthX(parameters.geometry.lengthX), _lengthY(parameters.geometry.lengthY),
   _lengthZ(parameters.geometry.dim==3 ? parameters.geometry.lengthZ : 0.0),
   _sizeX(parameters.geometry.sizeX), _sizeY(parameters.geometry.sizeY),
   _sizeZ(parameters.geometry.dim==3 ? parameters.geometry.sizeZ : 1),
   _firstCornerX(parameters.parallel.firstCorner[0]), _firstCornerY(parameters.parallel.firstCorner[1]),
   _firstCornerZ(parameters.geometry.dim==3 ? parameters.parallel.firstCorner[2] : 0),
   _stretchX(stretchX), _stretchY(stretchY), _stretchZ(stretchZ),
   _deltaS(deltaS), _tanhDeltaS(tanh(deltaS)),
   _dxMin(stretchX ? 0.5*parameters.geometry.lengthX*(1.0 + tanh(deltaS*(2.0/parameters.geometry.sizeX-1.0))/tanh(deltaS)) : _uniformMeshsize.getDx(0,0,0)),
   _dyMin(stretchY ? 0.5*parameters.geometry.lengthY*(1.0 + tanh(deltaS*(2.0/parameters.geometry.sizeY-1.0))/tanh(deltaS)) : _uniformMeshsize.getDy(0,0,0)),
   _dzMin(stretchZ ? 0.5*parameters.geometry.lengthZ*(1.0 + tanh(deltaS*(2.0/parameters.geometry.sizeZ-1.0))/tanh(deltaS)) : _uniformMeshsize.getDz(0,0,0))
{ }

TanhMeshStretching::~TanhMeshStretching(){
  free(_coordinatesX);
  free(_coordinatesY);
  free(_coordinatesZ);
}

void TanhMeshStretching::precomputeCoordinates(){
  // + 4 is needed to also get the height of the last cell (+ 3 due to the three ghost layers)
  if (_stretchX){
    _coordinatesX = (FLOAT*) malloc((_parameters.parallel.localSize[0] + 4) * sizeof(FLOAT));
    for (int i = 0; i < _parameters.parallel.localSize[0] + 4; i++) {
      _coordinatesX[i] = computeCoordinateX(i);
    }
  }
  if (_stretchY){
    _coordinatesY = (FLOAT*) malloc((_parameters.parallel.localSize[1] + 4) * sizeof(FLOAT));
    for (int i = 0; i < _parameters.parallel.localSize[1] + 4; i++) {
      _coordinatesY[i] = computeCoordinateY(i);
    }
  }
  if (_stretchZ){
    _coordinatesZ = (FLOAT*) malloc((_parameters.parallel.localSize[2] + 4) * sizeof(FLOAT));
    for (int i = 0; i < _parameters.parallel.localSize[2] + 4; i++) {
      _coordinatesZ[i] = computeCoordinateZ(i);
    }
  }
}

FLOAT TanhMeshStretching::computeCoordinate(int i, int firstCorner,int size, FLOAT length, FLOAT dxMin) const {
  const int index = i-2+firstCorner;
  // equidistant mesh on lower/left part
  if (index < 0){
    return dxMin*index;
  // equidistant mesh on upper/right part
  } else if (index > size-1){
    return length+dxMin*(index-size);
  } else {
    // stretched mesh on lower half of channel -> we check if we are in lower 50% and then use stretching for 2.0*p
    FLOAT p = ((FLOAT) index)/size;
    if (p<0.5){
      return 0.5*length*(1.0 + tanh(_deltaS*(2.0*p-1.0))/_tanhDeltaS);
    // stretched mesh on upper half of channel -> we mirror the stretching
    } else {
      p = ((FLOAT) size-index)/size;
      return length-0.5*length*(1.0 + tanh(_deltaS*(2.0*p-1.0))/_tanhDeltaS);
    }
  }
}

FLOAT TanhMeshStretching::computeCoordinateX(int i) const {
  return computeCoordinate(i,_firstCornerX,_sizeX,_lengthX,_dxMin);
}

FLOAT TanhMeshStretching::computeCoordinateY(int i) const {
  return computeCoordinate(i,_firstCornerY,_sizeY,_lengthY,_dyMin);
}

FLOAT TanhMeshStretching::computeCoordinateZ(int i) const {
  return computeCoordinate(i,_firstCornerZ,_sizeZ,_lengthZ,_dzMin);
}



BfsMeshStretching::BfsMeshStretching(
  const Parameters & parameters, const FLOAT deltaS
): TanhMeshStretching(parameters,true,true,true,deltaS)
{ }

BfsMeshStretching::~BfsMeshStretching(){}

FLOAT BfsMeshStretching::getDxMin() const {
  return (1.0 - _parameters.bfStep.xRatio) * _lengthX / ((int) ((1.0 - _parameters.bfStep.xRatio) * _sizeX));
}

void BfsMeshStretching::precomputeCoordinates(){
  _dyMinBelow = 1.0;
  _dyMinAbove = 0.0;
  _sizeYBelowStep = _parameters.bfStep.yRatio * _sizeY - 1;
  _sizeYAboveStep = _sizeY - _sizeYBelowStep;
  while(_dyMinAbove <= _dyMinBelow) {
    _sizeYBelowStep++;
    _sizeYAboveStep--;
    _dyMinBelow = 0.5*_parameters.bfStep.yRatio*_lengthY*(1.0 + tanh(_deltaS*(2.0/_sizeYBelowStep-1.0))/_tanhDeltaS);
    _dyMinAbove = 0.5*(1.0-_parameters.bfStep.yRatio)*_lengthY*(1.0 + tanh(_deltaS*(2.0/_sizeYAboveStep-1.0))/_tanhDeltaS);
  }

  TanhMeshStretching::precomputeCoordinates();
}

FLOAT BfsMeshStretching::computeCoordinateX(int i) const {
  const int index = i - 2 + _firstCornerX;
  const int nOnStep = _parameters.bfStep.xRatio * _sizeX;
  if (index < nOnStep){
    return index * _parameters.bfStep.xRatio * _lengthX / nOnStep;
  }else{
    return _parameters.bfStep.xRatio * _lengthX + (index - nOnStep) * (1.0 - _parameters.bfStep.xRatio) * _lengthX / (_sizeX - nOnStep);
  }
}

FLOAT BfsMeshStretching::computeCoordinateY(int i) const {
  const int index = i - 2 + _firstCornerY;
  if (index < _sizeYBelowStep){
    return computeCoordinate(i,_firstCornerY,_sizeYBelowStep,_parameters.bfStep.yRatio*_lengthY,_dyMinBelow);
  }else{
    return _parameters.bfStep.yRatio*_lengthY + computeCoordinate(i-_sizeYBelowStep,_firstCornerY,_sizeYAboveStep,(1.0-_parameters.bfStep.yRatio)*_lengthY,_dyMinAbove);
  }
}
