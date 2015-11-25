#include "VelocityBufferFillStencil.h"

VelocityBufferFillStencil::VelocityBufferFillStencil(const Parameters & parameters,
        FLOAT * velocitiesLeft, FLOAT * velocitiesRight,
        FLOAT * velocitiesBottom, FLOAT * velocitiesTop) :
    BoundaryStencil<FlowField>(parameters),
    _velocitiesLeft(velocitiesLeft), _velocitiesRight(velocitiesRight),
    _velocitiesBottom(velocitiesBottom), _velocitiesTop(velocitiesTop) {}

VelocityBufferFillStencil::VelocityBufferFillStencil(const Parameters & parameters,
        FLOAT * velocitiesLeft, FLOAT * velocitiesRight,
        FLOAT * velocitiesBottom, FLOAT * velocitiesTop,
        FLOAT * velocitiesFront, FLOAT * velocitiesBack) :
    BoundaryStencil<FlowField>(parameters),
    _velocitiesLeft(velocitiesLeft), _velocitiesRight(velocitiesRight),
    _velocitiesBottom(velocitiesBottom), _velocitiesTop(velocitiesTop),
    _velocitiesFront(velocitiesFront), _velocitiesBack(velocitiesBack) {}


// 2D problem
void VelocityBufferFillStencil::applyStencil2D(FlowField & flowField, FLOAT * velBuffer, int i, int j, int ind) {

    // Save pointer to avoid multiple calls to getVector()
    FLOAT * vel = flowField.getVelocity().getVector(i, j);

    #pragma unroll(2)
    for(int dim = 0; dim < 2; dim++) {
        velBuffer[ind*2 + dim] = vel[dim];
    }
}

void VelocityBufferFillStencil::applyLeftWall   ( FlowField & flowField, int i, int j ) {
    applyStencil2D(flowField, _velocitiesLeft, i, j, j);
}
void VelocityBufferFillStencil::applyRightWall  ( FlowField & flowField, int i, int j ) {
    applyStencil2D(flowField, _velocitiesRight, i, j, j);
}
void VelocityBufferFillStencil::applyBottomWall ( FlowField & flowField, int i, int j ) {
    applyStencil2D(flowField, _velocitiesBottom, i, j, i);
}
void VelocityBufferFillStencil::applyTopWall    ( FlowField & flowField, int i, int j ) {
    applyStencil2D(flowField, _velocitiesTop, i, j, i);
}


// 3D problem

// TODO Check if array index (ind) is right
inline void VelocityBufferFillStencil::applyStencil3D(FlowField & flowField, FLOAT * velBuffer, int i, int j, int k, int dimFast, int indSlow, int indFast) {

    // Save pointer to avoid multiple calls to getVector()
    FLOAT * vel = flowField.getVelocity().getVector(i, j, k);
    int ind = (indSlow * (_parameters.parallel.localSize[dimFast] + 2) + indFast) * 3;

    #pragma unroll(3)
    for(int dim = 0; dim < 3; dim++) {
        velBuffer[ind + dim] = vel[dim];
    }
}

void VelocityBufferFillStencil::applyLeftWall   ( FlowField & flowField, int i, int j, int k ) {
    applyStencil3D(flowField, _velocitiesLeft, i, j, k, 2, j, k);
}
void VelocityBufferFillStencil::applyRightWall  ( FlowField & flowField, int i, int j, int k ) {
    applyStencil3D(flowField, _velocitiesRight, i, j, k, 2, j, k);
}
void VelocityBufferFillStencil::applyBottomWall ( FlowField & flowField, int i, int j, int k ) {
    applyStencil3D(flowField, _velocitiesBottom, i, j, k, 2, i, k);
}
void VelocityBufferFillStencil::applyTopWall    ( FlowField & flowField, int i, int j, int k ) {
    applyStencil3D(flowField, _velocitiesTop, i, j, k, 2, i, k);
}
void VelocityBufferFillStencil::applyFrontWall  ( FlowField & flowField, int i, int j, int k ) {
    applyStencil3D(flowField, _velocitiesFront, i, j, k, 1, i, j);
}
void VelocityBufferFillStencil::applyBackWall   ( FlowField & flowField, int i, int j, int k ) {
    applyStencil3D(flowField, _velocitiesBack, i, j, k, 1, i, j);
}