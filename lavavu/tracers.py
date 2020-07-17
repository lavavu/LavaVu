"""
Warning! EXPERIMENTAL:
 these features and functions are under development, will have bugs,
 and may be heavily modified in the future

Tracer particles in a vector field
Uses a KDTree to find nearest vector to advect the particles

- Requires scipy.spatial
"""
import numpy
import os
import sys
import random
import math
from scipy import spatial

#Necessary? for large trees, detect?
#sys.setrecursionlimit(10000)

class TracerState(object):
    def __init__(self, verts, N=5000):
        self.tree = spatial.cKDTree(verts)
        self.tracers = None
        self.steps = [0]*N
        self.values = None
        self.positions = None
        self.velocities = None

def trace_particles(state, verts, vecs, N=5000, limit=0.5, speed=1.0, noise=0.0, height=None):
    """
    Take a list of tracer vertices and matching velocity grid points (verts) & vectors (vecs)
    For each tracer

    - find the nearest velocity grid point
    - if within max dist: Multiply position by velocity vector
    - otherwise: Generate a new start position for tracer

    Parameters
    ----------
    state : TracerState
        Object returned from first call, pass None on first pass
    verts : array or list
        vertices of the vector field
    vecs : array or list
        vector values of the vector field
    N : int
        Number of particles to seed
    limit : float
        Distance limit over which tracers are not connected,
        For example if using a periodic boundary, setting limit to
        half the bounding box size will prevent tracer lines being
        connected when passing through the boundary
    speed : float
        Speed multiplier, scaling factor for the velocity taken from the vector values
    noise : float
        A noise factor, if set a random value is generated, multiplied by noise factor
        and added to each new position
    height : float
        A fixed height value, all positions will be given this height as their Z component

    Returns
    -------
    TracerState
        Object to hold the tracer state and track particles
        Pass this as first paramter on subsequent calls
    """

    #KDstate.tree for finding nearest velocity grid point
    if state is None:
        state = TracerState(verts, N)

    lastid = 0
    def rand_vert():
        #Get a random velocity grid point
        lastid = random.randint(0, len(verts)-1)
        pos = verts[lastid]
        #Generate some random noise to offset
        noise3 = numpy.array((0.,0.,0.))
        if noise > 0.0:
            noise3 = numpy.random.rand(3) * noise
        #Fixed height?
        if height:
            noise3[2] = height
        #Return the sum
        return pos + noise3

    if state.positions is None:
        state.positions = numpy.zeros(shape=(N,3))
        state.velocities = numpy.zeros(shape=(N,3))
        state.values = numpy.zeros(shape=(N))
        for i in range(N):
            state.positions[i] = rand_vert()
            state.steps[i] = 0
            state.values[i] = numpy.linalg.norm(vecs[lastid])

    #Query all tracer state.positions
    q = state.tree.query(state.positions, k=1)
    for r in range(len(q[0])):
        #print("result, distance, point")
        #print(r, q[0][r], state.tree.data[q[1][r]], state.positions[r])

        #Increasing random chance as steps exceed 5 of a new start pos
        if random.randint(0,state.steps[r]) > 5:
            #Pick a new random grid vertex to start from
            #(Must be farther away than distance criteria)
            old = numpy.array(state.positions[r])
            while True:
                state.positions[r] = rand_vert() 
                if numpy.linalg.norm(state.positions[r]-old) > limit:
                    break
            state.steps[r] = 0
            state.values[r] = numpy.linalg.norm(vecs[lastid])
        else:
            #Index of nearest grid point is in q[1][r]
            #Lookup vector at this index, multiply by position to get delta and add
            state.velocities[r] = vecs[q[1][r]] #Store velocity
            state.positions[r] += speed * vecs[q[1][r]]
            #Increment step tracking
            state.steps[r] += 1
            state.values[r] = numpy.linalg.norm(vecs[q[1][r]])

    return state

