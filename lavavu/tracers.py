"""
Tracer particles in a vector field

- Requires scipy.interpolate
"""
import numpy
import os
import sys
import random
from scipy.interpolate import RegularGridInterpolator

def random_particles(count, lowerbound=[0,0,0], upperbound=[1,1,1], dims=3):
    """
    Return an array of *count* 3d vertices of random particle positions
    Minimum and maximum values defined by lowerbound and upperbound
    """
    p = [None] * dims
    for c in range(dims):
        if lowerbound[c] == upperbound[c]:
            p[c] = numpy.zeros(shape=(count)) + lowerbound[c]
        else:
            p[c] = numpy.random.uniform(low=lowerbound[c], high=upperbound[c], size=count)

    return numpy.stack(p).T

class Tracers():
    def __init__(self, grid, count=1000, lowerbound=None, upperbound=None, limit=None, age=4, respawn_chance=0.2, speed_multiply=1.0, height=0.0, viewer=None):
        """
        Seed random particles into a vector field and trace their positions

        Parameters
        ----------
        grid : list of coord arrays for each dimension as expected by RegularGridInterpolator,
            or a numpy array of 2d or 3d vertices, which will be converted before being sent to the interpolator
            Object returned from first call, pass None on first pass
        count : int
            Number of particles to seed and track
        lowerbound : optional minimum vertex point defining particle bounding box,
            if not provided will be taken from grid lower corner
        upperbound : optional maximum vertex point defining particle bounding box,
            if not provided will be taken from grid upper corner
        limit : float
            Distance limit over which tracers are not connected,
            For example if using a periodic boundary, setting limit to
            half the bounding box size will prevent tracer lines being
            connected when passing through the boundary
        age : int
            Minimum particle age in steps after which particle can be deleted and respawned, defaults to 4
        respawn : float
            Probability of respawning, after age reached, default 0.2 ==> 1 in 5 chance of deletion
        speed_multiply : float
            Speed multiplier, scaling factor for the velocity taken from the vector values
        height : float
            A fixed height value, all positions will be given this height as their Z component
        viewer : lavavu.Viewer
            Viewer object for plotting functions
        """
        if len(grid) == 2:
            self.gridx = grid[0]
            self.gridy = grid[1]
            self.gridz = numpy.array((height, height))
            self.dims = 2
        elif len(grid) == 3:
            self.gridx = grid[0]
            self.gridy = grid[1]
            self.gridz = grid[2]
            self.dims = 3
        elif isinstance(grid, numpy.ndarray) and grid.shape[1] == 3:
            self.gridx = grid[::,0]
            self.gridy = grid[::,1]
            self.gridz = grid[::,2]
            self.dims = 3
        elif isinstance(grid, numpy.ndarray) and grid.shape[1] == 2:
            self.gridx = grid[::,0]
            self.gridy = grid[::,1]
            self.gridz = numpy.array((height, height))
            self.dims = 2
        else:
            raise(ValueError('Grid needs to be array of 2d/3d vertices, or arrays of vertex coords (x, y, [z])'))

        self.count = count
        if lowerbound is None:
            lowerbound = (self.gridx[0], self.gridy[0], self.gridz[0])
        if upperbound is None:
            upperbound = (self.gridx[-1], self.gridy[-1], self.gridz[-1])
        self.positions = random_particles(self.count, lowerbound, upperbound, self.dims)
        self.old_pos = numpy.zeros_like(self.positions)
        self.lowerbound = lowerbound
        self.upperbound = upperbound
        self.velocities = None
        self.steps = [0]*count
        self.speed = numpy.zeros(shape=(count))
        self.ages = numpy.zeros(shape=(count))
        self.interp = None
        if limit is None:
            limit = 0.1 * (abs(self.gridx[-1] - self.gridx[0]) + abs(self.gridy[-1] - self.gridy[0]))
        self.limit = limit
        self.age = age
        self.respawn_chance = respawn_chance
        self.speed_multiply = speed_multiply
        self.height = height

        self.lv = viewer
        self.points = None
        self.arrows = None
        self.tracers = None


    def respawn(self, r):
        #Dead or out of bounds particle, start at new position
        #Loop until new position further from current position than limit
        old_pos = self.positions[r]
        pos = numpy.array([0.] * self.dims)
        for i in range(10):
            pos = random_particles(1, self.lowerbound, self.upperbound, self.dims)
            dist = numpy.linalg.norm(old_pos - pos)
            if dist > self.limit*1.01:
                break

        self.ages[r] = 0
        self.positions[r] = pos

    def update(self, vectors=None):
        #Interpolate velocity at all positions,
        #If vectors not passed, will use previous values
        if vectors is not None:
            if self.dims == 2:
                self.interp = RegularGridInterpolator((self.gridx, self.gridy), vectors, bounds_error=False, fill_value=0.0)
            else:
                self.interp = RegularGridInterpolator((self.gridx, self.gridy, self.gridz), vectors, bounds_error=False, fill_value=0.0)

        if self.interp is None:
            raise(ValueError("No velocity grid, must pass vectors for first call of update()"))

        self.velocities = self.interp(self.positions)
        self.old_pos = numpy.copy(self.positions)

        for r in range(len(self.velocities)):
            #Lookup velocity at this index, multiply by position to get delta and add
            self.speed[r] = numpy.linalg.norm(self.velocities[r])
            if numpy.isnan(self.speed[r]): self.speed[r] = 0.0
            if self.speed[r] == 0.0: #numpy.any(numpy.isinf(self.old_pos[r])) or numpy.any(numpy.isinf(self.positions[r])):
                self.respawn(r)
            else:
                self.positions[r] = self.positions[r] + self.speed_multiply * self.velocities[r]
                self.ages[r] += 1

            #Bounds checks
            #Chance of killing particle when over age, default 1 in 5 (0.2)
            if (any(self.positions[r] < self.lowerbound[0:self.dims]) or any(self.positions[r] > self.upperbound[0:self.dims])
            or (self.ages[r] > self.age and numpy.random.uniform() <= self.respawn_chance)):
                #if r < 20: print("Kill", r, self.speed[r], numpy.isnan(self.speed[r])) # [0] == numpy.nan)
                #self.positions[r] = numpy.array([numpy.inf] * self.dims)
                #self.positions[r] = numpy.array([numpy.nan] * self.dims)
                self.respawn(r)
                self.velocities[r] = numpy.array([0.0] * self.dims)
                self.speed[r] = 0.0

        if self.lv:
            positions = self.positions
            if self.dims == 2 and self.height != 0:
                #Convert to 3d and set z coord to height
                shape = list(positions.shape)
                shape[-1] = 3
                positions = numpy.zeros(shape)
                positions[::,0:2] = self.positions
                positions[::,2] = numpy.array([self.height] * shape[0])
            if self.points:
                self.points.vertices(positions)
                if len(self.points["colourmap"]):
                    self.points.values(self.speed)
            if self.arrows:
                self.arrows.vectors(self.velocities)
                self.arrows.vertices(positions)
                if len(self.arrows["colourmap"]):
                    self.arrows.values(self.speed)

            if self.tracers:
                self.tracers.vertices(positions)
                if len(self.tracers["colourmap"]):
                    self.tracers.values(self.speed)

    def plot_points(self, **kwargs):
        if self.lv is not None and self.points is None:
            self.points = self.lv.points('tracer_points', **kwargs)

    def plot_arrows(self, **kwargs):
        if self.lv is not None and self.arrows is None:
            self.arrows = self.lv.vectors('tracer_arrows', **kwargs)

    def plot_tracers(self, **kwargs):
        if self.lv is not None and self.tracers is None:
            self.tracers = self.lv.tracers('tracers', dims=self.count, limit=self.limit, **kwargs)

