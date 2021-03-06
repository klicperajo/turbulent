#include "VelocityBufferReadStencil.h"

VelocityBufferReadStencil::VelocityBufferReadStencil(const Parameters & parameters,
        FLOAT * velocitiesLeft, FLOAT * velocitiesRight,
        FLOAT * velocitiesBottom, FLOAT * velocitiesTop,
        int lowOffset) :
    BoundaryStencil<FlowField>(parameters),
    _velocitiesLeft(velocitiesLeft), _velocitiesRight(velocitiesRight),
    _velocitiesBottom(velocitiesBottom), _velocitiesTop(velocitiesTop),
    _lowOffset(lowOffset) {}

VelocityBufferReadStencil::VelocityBufferReadStencil(const Parameters & parameters,
        FLOAT * velocitiesLeft, FLOAT * velocitiesRight,
        FLOAT * velocitiesBottom, FLOAT * velocitiesTop,
        FLOAT * velocitiesFront, FLOAT * velocitiesBack,
        int lowOffset) :
    BoundaryStencil<FlowField>(parameters),
    _velocitiesLeft(velocitiesLeft), _velocitiesRight(velocitiesRight),
    _velocitiesBottom(velocitiesBottom), _velocitiesTop(velocitiesTop),
    _velocitiesFront(velocitiesFront), _velocitiesBack(velocitiesBack),
    _lowOffset(lowOffset) {}


// 2D problem
void VelocityBufferReadStencil::applyStencil2D(FlowField & flowField, FLOAT * velBuffer, int i, int j, int ind) {

    // Save pointer to avoid multiple calls to getVector()
    FLOAT * vel = flowField.getVelocity().getVector(i, j);

    #pragma unroll(2)
    for(int dim = 0; dim < 2; dim++) {
        vel[dim] = velBuffer[ind*2 + dim];
    }
}

void VelocityBufferReadStencil::applyLeftWall   ( FlowField & flowField, int i, int j ) {
    // applyStencil2D(flowField, _velocitiesLeft, i-1, j, j);
    applyStencil2D(flowField, _velocitiesLeft, i+1, j, 2*j+1);
    applyStencil2D(flowField, _velocitiesLeft, i,   j, 2*j);
}
void VelocityBufferReadStencil::applyRightWall  ( FlowField & flowField, int i, int j ) {
    // applyStencil2D(flowField, _velocitiesRight, i+1, j, j);
    applyStencil2D(flowField, _velocitiesRight, i, j, j);
}
void VelocityBufferReadStencil::applyBottomWall ( FlowField & flowField, int i, int j ) {
    // applyStencil2D(flowField, _velocitiesBottom, i, j-1, i);
    applyStencil2D(flowField, _velocitiesBottom, i, j+1, 2*i+1);
    applyStencil2D(flowField, _velocitiesBottom, i, j,   2*i);
}
void VelocityBufferReadStencil::applyTopWall    ( FlowField & flowField, int i, int j ) {
    // applyStencil2D(flowField, _velocitiesTop, i, j+1, i);
    applyStencil2D(flowField, _velocitiesTop, i, j, i);
}


// 3D problem

void VelocityBufferReadStencil::applyStencil3D(FlowField & flowField, FLOAT * velBuffer, int i, int j, int k, int dimFast, int indSlow, int indFast) {

    // Save pointer to avoid multiple calls to getVector()
    FLOAT * vel = flowField.getVelocity().getVector(i, j, k);
    int ind = (indSlow * (_parameters.parallel.localSize[dimFast] + 2) + indFast) * 3;

    #pragma unroll(3)
    for(int dim = 0; dim < 3; dim++) {
        vel[dim] = velBuffer[ind + dim];
    }
}

void VelocityBufferReadStencil::applyLeftWall   ( FlowField & flowField, int i, int j, int k ) {
    // applyStencil3D(flowField, _velocitiesLeft, i, j, k, 2, j, k);
    applyStencil3D(flowField, _velocitiesLeft, i+1, j, k, 2, 2*j+1, k);
    applyStencil3D(flowField, _velocitiesLeft, i, j, k, 2, 2*j, k);
}
void VelocityBufferReadStencil::applyRightWall  ( FlowField & flowField, int i, int j, int k ) {
    applyStencil3D(flowField, _velocitiesRight, i, j, k, 2, j, k);
}
void VelocityBufferReadStencil::applyBottomWall ( FlowField & flowField, int i, int j, int k ) {
    // applyStencil3D(flowField, _velocitiesBottom, i, j, k, 2, i, k);
    applyStencil3D(flowField, _velocitiesBottom, i, j+1, k, 2, 2*i+1, k);
    applyStencil3D(flowField, _velocitiesBottom, i, j, k, 2, 2*i, k);
}
void VelocityBufferReadStencil::applyTopWall    ( FlowField & flowField, int i, int j, int k ) {
    applyStencil3D(flowField, _velocitiesTop, i, j, k, 2, i, k);
}
void VelocityBufferReadStencil::applyFrontWall  ( FlowField & flowField, int i, int j, int k ) {
    // applyStencil3D(flowField, _velocitiesFront, i, j, k, 1, i, j);
    applyStencil3D(flowField, _velocitiesFront, i, j, k+1, 1, 2*i+1, j);
    applyStencil3D(flowField, _velocitiesFront, i, j, k, 1, 2*i, j);
}
void VelocityBufferReadStencil::applyBackWall   ( FlowField & flowField, int i, int j, int k ) {
    applyStencil3D(flowField, _velocitiesBack, i, j, k, 1, i, j);
}
