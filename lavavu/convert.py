"""
Warning! EXPERIMENTAL:
 these features and functions are under development, will have bugs,
 and may be heavily modified in the future

Tools for converting between 3D data types

- Points to Volume
- Triangles to OBJ file
"""
import numpy
import os

def min_max_range(verts):
    """
    Get bounding box from a list of vertices
    returns (min, max, range)
    """
    vmin = numpy.min(verts, axis=0)
    vmax = numpy.max(verts, axis=0)
    vrange = vmax - vmin
    print("Bounding box ", (vmin, vmax), "Range ", vrange)
    return (vmin, vmax, vrange)

def default_sample_grid(vrange, res=8):
    """Calculate sample grid fine enough to capture point details
    resolution of smallest dimension will be minres elements
    """
    #If provided a full resolution, use that, otherwise will be interpreted as min res
    if isinstance(res, list) and len(res) == 3:
        return res
    #Use bounding box range min
    minr = numpy.min(vrange)
    #res parameter is minimum resolution
    factor = float(res) / float(minr)
    RES = [int(factor*(vrange[0])), int(factor*(vrange[1])), int(factor*(vrange[2]))]   
    print("Sample grid RES:",RES)
    return RES

def points_to_volume(verts, res=8, kdtree=False, normed=False, clamp=None, boundingbox=None):
    """
    Convert object vertices to a volume by interpolating points to a grid

    - Vertex data only is used, so treated as a point cloud
    - Can also be used to sample an irregular grid to a regular so volume render matches the grid dimensions
    - Result is a density field that can be volume rendered

    Default is to use numpy.histogramdd method, pass kdtree=True to use scipy.spatial.KDTree

    TODO: support colour data too, converted density field becomes alpha channel

    Returns:
    --------
    3tuple containing (values, vmin, vmax):

    values: numpy array of float32
        The converted density field
    vmin:
        the minimum 3d vertex of the bounding box
    vmax:
        the maximum 3d vertex of the bounding box

    """
    if kdtree:
        return points_to_volume_tree(verts, res)
    else:
        return points_to_volume_histogram(verts, res, normed, clamp, boundingbox)

def points_to_volume_histogram(verts, res=8, normed=False, clamp=None, boundingbox=None):
    """
    Using numpy.histogramdd to create 3d histogram volume grid
    (Easily the fastest, but less control over output)
    """
    #Reshape to 3d vertices
    verts = verts.reshape((-1,3))

    #Get bounding box of swarm
    vmin, vmax, vrange = min_max_range(verts)

    #Minimum resolution to get ok sampling
    if boundingbox is not None:
        vmin = boundingbox[0]
        vmax = boundingbox[1]
        vrange = numpy.array(boundingbox[1]) - numpy.array(boundingbox[0])
    RES = default_sample_grid(vrange, res)

    #H, edges = numpy.histogramdd(verts, bins=RES)
    if boundingbox is None:
        H, edges = numpy.histogramdd(verts, bins=RES, normed=normed) #density=True for newer numpy
    else:
        rg = ((vmin[0], vmax[0]), (vmin[1], vmax[1]), (vmin[2], vmax[2])) #provide bounding box as range
        H, edges = numpy.histogramdd(verts, bins=RES, range=rg, normed=normed) #density=True for newer numpy

    #Reverse ordering X,Y,Z to Z,Y,X for volume data
    values = H.transpose()

    #Clamp [0,0.1] - optional (for a more binary output, points vs no points)
    if clamp is not None:
        values = numpy.clip(values, a_min=clamp[0], a_max=clamp[1])
    
    #Normalise [0,1] if using probability density
    if normed:
        values = values / numpy.max(values)
    
    return (values, vmin, vmax)

def points_to_volume_tree(verts, res=8):
    """
    Using scipy.spatial.KDTree to find nearest points on grid

    - Much slower, but more control
    TODO: control parameters
    """
    #Reshape to 3d vertices
    verts = verts.reshape((-1,3))

    #Get bounding box of swarm
    lmin, lmax, lrange = min_max_range(verts)
    print("Volume bounds: ",lmin.tolist(), lmax.tolist(), lrange.tolist())
    
    #Push out the edges a bit, will create a smoother boundary when we filter
    #lmin -= 0.1*lrange
    #lmax += 0.1*lrange
    values = numpy.full(shape=(verts.shape[0]), dtype=numpy.float32, fill_value=1.0)

    #Minimum resolution to get ok sampling
    RES = default_sample_grid(lrange, res)

    #Push out the edges a bit, will create a smoother boundary when we filter
    cell = lrange / RES #Cell size
    print("CELL:",cell)
    lmin -= 2.0*cell
    lmax += 2.0*cell

    x = numpy.linspace(lmin[0], lmax[0] , RES[0])
    y = numpy.linspace(lmin[1], lmax[1] , RES[1])
    z = numpy.linspace(lmin[2], lmax[2] , RES[2])
    print(x.shape, y.shape, z.shape)
    Z, Y, X = numpy.meshgrid(z, y, x, indexing='ij') #ij=matrix indexing, xy=cartesian
    XYZ = numpy.vstack((X,Y,Z)).reshape([3, -1]).T

    print(lmin,lmax, XYZ.shape)

    #KDtree for finding nearest neighbour points
    from scipy import spatial
    #sys.setrecursionlimit(1000)
    #tree = spatial.KDTree(XYZ)
    print("Building KDTree")
    tree = spatial.cKDTree(XYZ)

    #Outside distance to apply to grid points
    MAXDIST = max(lrange) / max(RES) #Max cell size diagonal
    print("Tree query, maxdist:",MAXDIST,max(lrange))

    #Query all points, result is tuple: distances, indices
    distances, indices = tree.query(verts, k=1) #Just get a single nearest neighbour for now

    print("Calculate distances")
    #Convert distances to [0,1] where 1=on grid and <= 0 is outside max range
    distances = (MAXDIST - distances) / MAXDIST
    print("Zero out of range distances")
    distances *= (distances>0) #Zero negative elements by multiplication in-place

    #Add the distances to the values at their nearest grid point indices
    print("Compose distance field")
    values = numpy.zeros(shape=(XYZ.shape[0]), dtype=numpy.float32)
    values[indices] += distances
    #Clip value max
    print("Clip distance field")
    values = values.clip(max=1.0)
    
    #Reshape to actual grid dims Z,Y,X... (not required but allows slicing)
    XYZ = XYZ.reshape(RES[::-1] + [3])
    values = values.reshape(RES[::-1])
    print(XYZ.shape, values.shape)
    
    return (values, lmin, lmax)


def points_to_volume_3D(vol, objects, res=8, kdtree=False, blur=False, normed=False, clamp=None):
    """
    Interpolate points to grid and load into passed volume object

    Given a list of objects and a volume object, convert a point cloud
    from another object (or objects - list is supported) into a volume using
    points_to_volume()
    """
    lv = vol.parent #Get the viewer from passed object
    lv.hide(objects) #Hide the converted objects

    #Get vertices from lavavu objects
    pverts, bb_all = lv.get_all_vertices(objects)

    #blur = False
    #Use bounding box of full model?
    #vmin, vmax, vrange = min_max_range([lv["min"], lv["max"]])
    #bb_all == (vmin, vmax)
    vdata, vmin, vmax = points_to_volume(pverts, res, kdtree, normed, clamp, bb_all)

    if blur:
        print("Filter/blur distance field")
        from scipy.ndimage.filters import gaussian_filter
        values = gaussian_filter(vdata, sigma=1) #, mode='nearest')
    else:
        values = vdata #Need extra space at edges to use blur, so skip, or use pad() 
    
    print(numpy.min(values), numpy.max(values))
    print(numpy.min(values), numpy.max(values))
    
    vol.values(values)
    vol.vertices((vmin, vmax))

def points_to_volume_4D(vol, objects, res=8, kdtree=False, blur=False, normed=False, clamp=None):
    """
    Interpolate points to grid at each timestep

    Given a list of objects and a volume object, convert a time-varying point cloud
    from another object (or objects - list is supported) into a volume using
    points_to_volume_3D()
    """
    lv = vol.parent #Get the viewer from passed object

    for step in lv.steps:
        print("TIMESTEP:",step)
        lv.timestep(step)
        
        points_to_volume_3D(vol, objects, res, kdtree, blur, normed, clamp)


def export_OBJ(filepath, obj):
    """
    Export a given object to an OBJ file
    Supports only triangle mesh objects

    TODO: support colour texture by writing palette as png
    """
    mtl_line = ""
    mtl_line2 = ""
    cmaptexcoords = []
    if obj["texture"] == 'colourmap':
        #Write palette.png
        obj.parent.palette('image', 'texture')
        mtl = ("# Create as many materials as desired\n"
        "# Each is referenced by name before the faces it applies to in the obj file\n"
        "newmtl material0\n"
        "Ka 1.000000 1.000000 1.000000\n"
        "Kd 1.000000 1.000000 1.000000\n"
        "Ks 0.000000 0.000000 0.000000\n"
        "Tr 1.000000\n"
        "illum 1\n"
        "Ns 0.000000\n"
        "map_Kd texture.png\n")
        with open(filepath + '.mtl', 'w') as f:
            f.write(mtl)
        filename = os.path.basename(filepath)
        mtl_line = "mtllib " + filename + ".mtl\n"
        mtl_line2 = "usemtl material0\n"

    with open(filepath, 'w') as f:
        f.write("# OBJ file\n")
        #f.write(mtl_line)
        offset = 1
        for o in range(len(obj.data.vertices)):
            print("[%s] element %d of %d" % (obj.name, o+1, len(obj.data.vertices)))
            f.write("o Surface_%d\n" % o)
            f.write(mtl_line) #After this?
            verts = obj.data.vertices[o].reshape((-1,3))
            if len(verts) == 0:
                print("No vertices")
                continue
            indices = obj.data.indices[o].reshape((-1,3))
            normals = obj.data.normals[o].reshape((-1,3))
            texcoords = obj.data.texcoords[o].reshape((-1,2))
            #Calculate texcoords from colour values?
            if len(texcoords) == 0 and obj["texture"] == 'colourmap':
                label = obj["colourby"]
                if isinstance(label,int):
                    #Use the given label index
                    sets = list(obj.datasets.keys())
                    label = sets[label]
                elif len(label) == 0:
                    #Use the default label
                    label = 'values'
                valdata = obj.data[label]
                if len(valdata) >= o+1:
                    #Found matching value array
                    v = valdata[o]
                    #Normalise [0,1]
                    texcoords = (v - numpy.min(v)) / numpy.ptp(v)
                    #Add 2nd dimension (not actually necessary,
                    #tex coords can by 1d but breaks some loaders (meshlab)
                    zeros = numpy.zeros((len(texcoords)))
                    texcoords = numpy.vstack((texcoords,zeros)).reshape([2, -1]).transpose()

            print("- Writing vertices:",verts.shape)
            for v in verts:
                f.write("v %.6f %.6f %.6f\n" % (v[0], v[1], v[2]))
            print("- Writing normals:",normals.shape)
            for n in normals:
                f.write("vn %.6f %.6f %.6f\n" % (n[0], n[1], n[2]))
            print("- Writing texcoords:",texcoords.shape)
            if len(texcoords.shape) == 2:
                for t in texcoords:
                    f.write("vt %.6f %.6f\n" % (t[0], t[1]))
            else:
                for t in texcoords:
                    f.write("vt %.6f\n" % t)

            #Face elements v/t/n v/t v//n
            f.write(mtl_line2)
            if len(normals) and len(texcoords):
                print("- Writing faces (v/t/n):",indices.shape)
                for i in indices:
                    i0 = i[0]+offset
                    i1 = i[1]+offset
                    i2 = i[2]+offset
                    f.write("f %d/%d/%d %d/%d/%d %d/%d/%d\n" % (i0, i0, i0, i1, i1, i1, i2, i2, i2))
            elif len(texcoords):
                print("- Writing faces (v/t):",indices.shape)
                for i in indices:
                    i0 = i[0]+offset
                    i1 = i[1]+offset
                    i2 = i[2]+offset
                    f.write("f %d/%d %d/%d %d/%d\n" % (i0, i0, i1, i1, i2, i2))
            elif len(normals):
                print("- Writing faces (v//n):",indices.shape)
                for i in indices:
                    i0 = i[0]+offset
                    i1 = i[1]+offset
                    i2 = i[2]+offset
                    f.write("f %d//%d %d//%d %d//%d\n" % (i0, i0, i1, i1, i2, i2))
            else:
                print("- Writing faces (v):",indices.shape)
                for i in indices:
                    f.write("f %d %d %d\n" % (i[0]+offset, i[1]+offset, i[2]+offset))

            offset += verts.shape[0]

