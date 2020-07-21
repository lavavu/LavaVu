from __future__ import print_function
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
import sys

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
    if len(list(res)) == 3:
        return list(res)
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

    Returns
    -------
    values: numpy array of float32
        The converted density field
    boundingbox : 2,3
        the minimum 3d vertex of the bounding box
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

    Much slower, but more control

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


def points_to_volume_3D(vol, objects, res=8, kdtree=False, blur=0, pad=None, normed=False, clamp=None):
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

    if blur > 0:
        if pad == None:
            pad = int(blur*2)
        if pad > 0:
            print("Pad edges before blur", pad)
            vdata = numpy.pad(vdata, pad, mode='constant')
        print("Filter/blur distance field, sigma=%d" % blur)
        from scipy.ndimage.filters import gaussian_filter
        values = gaussian_filter(vdata, sigma=blur) #, mode='nearest')
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

def colour2rgb(c):
    return [c & 255, (c >> 8) & 255, (c >> 16) & 255]

def colour2hex(rgb):
    def padhex2(i):
        s = hex(int(i))
        return s[2:].zfill(2)
    return "#" + padhex2(rgb[0]) + padhex2(rgb[1]) + padhex2(rgb[2])

def _get_objects(source):
    """
    Returns a list of objects

    If source is lavavu.Viewer() list contains all objects
    If source is lavavu.Object() list contains that single object
    """
    if source.__class__.__name__ == 'Viewer':
        return source.objects.list
    elif not isinstance(source, list):
        return [source]
    else:
        return source

def export_OBJ(filepath, source, verbose=False):
    """
    Export given object(s) to an OBJ file
    Supports only triangle mesh object data

    If source is lavavu.Viewer() exports all objects
    If source is lavavu.Object() exports single object
    """
    mtlfilename = os.path.splitext(filepath)[0] + '.mtl'
    objects = _get_objects(source)
    with open(filepath, 'w') as f, open(mtlfilename, 'w') as m:
        f.write("# OBJ file\n")
        offset = 1
        for obj in objects:
            if obj["visible"]:
                f.write("g %s\n" % obj.name)
                offset = _write_OBJ(f, m, filepath, obj, offset, verbose)

def _write_MTL(m, name, texture=None, diffuse=[1.0, 1.0, 1.0], ambient=None, specular=None, opacity=1.0, illum=None):
    #http://paulbourke.net/dataformats/mtl/
    #print("Writing MTL ", texture, diffuse, opacity, name)
    mtl = "newmtl %s\n" % name
    mtl += "Kd %.06f %.06ff %.06f\n" % (diffuse[0], diffuse[1], diffuse[2])
    if ambient:
        mtl += "Ka %.06f %.06ff %.06f\n" % (ambient[0], ambient[1], ambient[2])
    if specular:
        if illum is None:
            illum = 2 #Highlight on
        mtl += "Ks %.06f %.06ff %.06f\n" % (specular[0], specular[1], specular[2])
        if len(specular) > 3:
            mtl += "Ns %.06f\n" % specular[3]
    mtl += "d %f\n" % (1.0 - opacity) #Dissolve: inverse of opacity
    if illum is None:
        illum = 1 #Default=1 = colour on, ambient on
    mtl += "illum %d\n" % illum

    if texture:
        mtl += "map_Kd %s\n" % texture

    mtl += '\n'
    m.write(mtl)
    mtl_line = "usemtl %s\n" % name
    return mtl_line

def _write_OBJ(f, m, filepath, obj, offset=1, verbose=False):
    mtl_line = ""
    cmaptexcoords = []
    colourdict = None
    import re
    name = re.sub(r"\s+", "", obj["name"])
    colourcount = sum([len(c) for c in obj.data.colours])
    if m and obj["texture"]:
        fn = obj["texture"]
        if fn == 'colourmap':
            fn = 'texture.png'
        #Write palette.png
        obj.parent.palette('image', 'texture')
        if verbose:
            print("Writing texture mtl ", fn)
        mtl_line = _write_MTL(m, name, texture=fn, opacity=obj["opacity"])

    elif m and colourcount > 0:
        #"""
        #18bit RGB = 262144 colours
        #Top-left = (0,0,0)
        #Bottom-right = (255,255,255)
        #(8x8 tiles of 64x64)
        #G - slow, Z index = G
        #R - midd, Y axis = R
        #B - fast, X axis = B
        #X = B//4
        #Y = R//4
        #Z = G//4
        #zx = Z % 8
        #zy = Z // 8
        #x = zx * 64 + X
        #y = zy * 64 + Y
        #u = x / 512
        #v = y / 512
        #https://upload.wikimedia.org/wikipedia/commons/3/34/RGB_18bits_palette.png
        palimg = u"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"
        import base64
        decoded = base64.b64decode(palimg).decode('utf-8')
        with open("palette_rgb.png", "wb") as f:
            f.write(decoded.encode('ascii'))
        mtl_line = _write_MTL(m, "palette_rgb", texture="palette_rgb.png", opacity=obj["opacity"])
        colourdict = {}
        """
        #SLOW!
        print("Creating palette")
        colourdict = {}
        for data in obj:
            for c in data.colours:
                rgb = colour2rgb(c)
                cs = colour2hex(rgb)
                colourdict[cs] = rgb
        print("Writing mtl lib (colour list)")
        for c in colourdict:
            rgb = colourdict[c]
            mtl_line = _write_MTL(m, c[1:], diffuse=[rgb[0]/255.0, rgb[1]/255.0, rgb[2]/255.0], opacity=obj["opacity"])
        """

    elif m and "colour" in obj:
        #print("Writing mtl lib (default colour)")
        c = obj.parent.parse_colour(obj["colour"])
        mtl_line = _write_MTL(m, 'default-' + name, diffuse=c, opacity=obj["opacity"])

    for o,data in enumerate(obj):
        if verbose: print("[%s] element %d of %d" % (obj.name, o+1, len(obj.data.vertices)))
        verts = data.vertices.reshape((-1,3))
        if len(verts) == 0:
            if verbose: print("No vertices")
            continue
        f.write("o Surface_%d\n" % o)
        #f.write("o %s\n" % obj.name)
        if m: f.write("mtllib " + os.path.splitext(os.path.basename(filepath))[0] + ".mtl\n")
        indices = data.indices.reshape((-1,3))
        normals = data.normals.reshape((-1,3))
        texcoords = data.texcoords.reshape((-1,2))
        #Calculate texcoords from colour values?
        if len(texcoords) == 0 and obj["texture"] == 'colourmap':
            label = obj["colourby"]
            if isinstance(label,int):
                #Use the given label index
                sets = list(datasets.keys())
                label = sets[label]
            elif len(label) == 0:
                #Use the default label
                label = 'values'
            valdata = data[label]
            if len(valdata) >= o+1:
                #Found matching value array
                v = valdata
                #Normalise [0,1]
                texcoords = (v - numpy.min(v)) / numpy.ptp(v)
                #Add 2nd dimension (not actually necessary,
                #tex coords can by 1d but breaks some loaders (meshlab)
                zeros = numpy.zeros((len(texcoords)))
                texcoords = numpy.vstack((texcoords,zeros)).reshape([2, -1]).transpose()

        #Colours?
        cs0 = ""
        vperc = 1
        if colourdict and len(data.colours):
            vperc = int(verts.shape[0] / len(data.colours))

        if verbose: print("- Writing vertices:",verts.shape)
        for v in verts:
            if colourdict:
                c = data.colours[ci]
                rgb = colour2rgb(c)
                f.write("v %.6f %.6f %.6f %.6f %.6f %.6f\n" % (v[0], v[1], v[2], rgb[0]/255.0, rgb[1]/255.0, rgb[2]/255.0))
            else:
                f.write("v %.6f %.6f %.6f\n" % (v[0], v[1], v[2]))
        if verbose: print("- Writing normals:",normals.shape)
        for n in normals:
            f.write("vn %.6f %.6f %.6f\n" % (n[0], n[1], n[2]))
        if verbose: print("- Writing texcoords:",texcoords.shape)
        if len(texcoords.shape) == 2:
            for t in texcoords:
                f.write("vt %.6f %.6f\n" % (t[0], t[1]))
        else:
            for t in texcoords:
                f.write("vt %.6f\n" % t)

        #Face elements v/t/n v/t v//n
        f.write(mtl_line)
        if len(normals) and len(texcoords):
            if verbose: print("- Writing faces (v/t/n):",indices.shape)
        elif len(texcoords):
            if verbose: print("- Writing faces (v/t):",indices.shape)
        elif len(normals):
            if verbose: print("- Writing faces (v//n):",indices.shape)
        else:
            if verbose: print("- Writing faces (v):",indices.shape)
        if verbose: print("- Colours :",data.colours.shape)
        if verbose: print("- Indices :",indices.shape)

        for n,i in enumerate(indices):
            if verbose and n%1000==0: print(".", end=''); sys.stdout.flush()
            i0 = i[0]+offset
            i1 = i[1]+offset
            i2 = i[2]+offset
            if colourdict:
                """
                ci = int(i[0] / vperc)
                #print(i0,vperc,ci)
                #print(data.colours)
                #c = data.colours[ci]
                c = data.colours[ci]
                rgb = colour2rgb(c)
                cs = colour2hex(rgb)
                cs1 = cs[1:]
                #if i[0]%100==0: print(ci,c,rgb,cs,mtl_line2)
                if cs0 != cs1 and cs in colourdict:
                    f.write("usemtl " + cs1 + "\n")
                    cs0 = cs1
                """

            if len(normals) and len(texcoords):
                f.write("f %d/%d/%d %d/%d/%d %d/%d/%d\n" % (i0, i0, i0, i1, i1, i1, i2, i2, i2))
            elif len(texcoords):
                f.write("f %d/%d %d/%d %d/%d\n" % (i0, i0, i1, i1, i2, i2))
            elif len(normals):
                f.write("f %d//%d %d//%d %d//%d\n" % (i0, i0, i1, i1, i2, i2))
            else:
                f.write("f %d %d %d\n" % (i0, i1, i2))
        if verbose: print()

        offset += verts.shape[0]
    return offset

def export_PLY(filepath, source, binary=True):
    """
    Export given object(s) to a PLY file
    Supports points or triangle mesh object data

    If source is lavavu.Viewer() exports all objects
    If source is lavavu.Object() exports single object

    Parameters
    ----------
    filepath : str
        Output file to write
    source : lavavu.Viewer or lavavu.Object
        Where to get object data to export
    binary : boolean
        Write vertex/face data as binary, default True
    """
    objects = _get_objects(source)
    with open(filepath, mode='wb') as f:
        voffset = 0
        foffset = 0
        #First count vertices, faces
        fc = 0
        vc = 0
        for obj in objects:
            for o,data in enumerate(obj):
                vc += len(data.vertices) #Vertices now in shape (N,3) so len returns correct count
                fc += len(data.indices) // 3

        #Pass the counts first
        vertex = None
        face = None
        print(vc, " vertices, ", fc, " faces")
        for obj in objects:
            for o,data in enumerate(obj):
                print("[%s] element %d of %d, type %s" % (obj.name, o+1, len(obj.data), data.type))
                #print("OFFSETS:",voffset,foffset)
                verts = data.vertices.reshape((-1,3))
                if len(verts) == 0:
                    print("No vertices")
                    return
                if len(verts) != vc:
                    print("Vertex count error!", len(verts), vc, verts.shape)
                    return
                indices = data.indices.reshape((-1,3))
                normals = data.normals.reshape((-1,3))
                texcoords = data.texcoords.reshape((-1,2))

                vperc = 0
                cperf = 0
                if len(data.colours):
                    vperc = int(verts.shape[0] / len(data.colours))
                    C = data.colours
                    #print("COLOURS:",len(C),C.shape, verts.shape[0], verts.shape[0] / len(data.colours), vperc)

                if data.type != 'points':
                    #Per face colours, or less
                    if face is None:
                        if vperc and vperc < len(verts):
                            #cperf = int(indices.shape[0] / len(data.colours))
                            face = numpy.zeros(shape=(fc), dtype=[('vertex_indices', 'i4', (3,)), ('red', 'u1'), ('green', 'u1'), ('blue', 'u1')])
                        else:
                            face = numpy.zeros(shape=(fc), dtype=[('vertex_indices', 'i4', (3,))])
                        print("FACE:",face.dtype)

                    for i,idx in enumerate(indices):
                        if i%1000==0: print(".", end=''); sys.stdout.flush()
                        if vperc and vperc < len(verts):
                            #Have colour, but less than vertices, apply to faces
                            ci = idx[0] // vperc
                            c = data.colours[ci]
                            rgb = colour2rgb(c)
                            face[i+foffset] = ([idx[0]+voffset, idx[1]+voffset, idx[2]+voffset], rgb[0], rgb[1], rgb[2])
                        else:
                            face[i+foffset] = ([idx[0]+voffset, idx[1]+voffset, idx[2]+voffset])
                    print()

                #Construct and write vertex elements
                if vertex is None:
                    #Setup vertex array based on first object element
                    D = [('x', 'f4'), ('y', 'f4'), ('z', 'f4')]
                    if normals.shape[0] == verts.shape[0]:
                        D += [('nx', 'f4'), ('ny', 'f4'), ('nz', 'f4')]
                    if texcoords.shape[0] == verts.shape[0]:
                        D += [('s', 'f4'), ('t', 'f4')]
                    if vperc and vperc == 1:
                        D += [('red', 'u1'), ('green', 'u1'), ('blue', 'u1')]
                    print("VERTEX:",D)
                    vertex = numpy.zeros(shape=(vc), dtype=D)

                for i,v in enumerate(verts):
                    #if i%1000==0:
                    #    print("vert index",i,vperc)
                    if i%1000==0: print(".", end=''); sys.stdout.flush()
                    E = [v[0], v[1], v[2]]
                    if normals.shape[0] == verts.shape[0]:
                        N = normals[i]
                        E += [N[0], N[1], N[2]]
                    if texcoords.shape[0] == verts.shape[0]:
                        T = texcoords[i]
                        E += [T[0], T[1]]
                    if vperc and vperc == 1:
                        c = data.colours[i]
                        rgb = colour2rgb(c)
                        E += [rgb[0], rgb[1], rgb[2]]
                    vertex[i+voffset] = tuple(E)
                print()

                #Update offsets : number of vertices / faces added
                voffset += verts.shape[0]
                foffset += indices.shape[0]

        import plyfile
        #vertex = numpy.array(vertex, dtype=vertex.dtype)
        els = []
        els.append(plyfile.PlyElement.describe(vertex, 'vertex'))
        if face is not None:
            els.append(plyfile.PlyElement.describe(face, 'face'))

        #Write, text or binary
        if binary:
            print("Writing binary PLY data")
            plyfile.PlyData(els).write(f)
        else:
            print("Writing ascii PLY data")
            plyfile.PlyData(els, text=True).write(f)

def _get_PLY_colours(element):
    """
    Extract colour data from PLY element and return as a numpy rgba array
    """
    r = None
    g = None
    b = None
    a = None
    #print(element.properties)
    for prop in element.properties:
        #print(prop,prop.name,prop.dtype)
        if 'red' in prop.name: r = element[prop.name]
        if 'green' in prop.name: g = element[prop.name]
        if 'blue' in prop.name: b = element[prop.name]
        if 'alpha' in prop.name: a = element[prop.name]

    if r is not None and g is not None and b is not None:
        if a is None:
            a = numpy.full(r.shape, 255)
        #return numpy.array([r, g, b, a])
        return numpy.vstack((r,g,b,a)).reshape([4, -1]).transpose()

    return None

def plot_PLY(lv, filename):
    """
    Plot triangles from a PlyData instance. Assumptions:
    has a 'vertex' element with 'x', 'y', and 'z' properties,
    has a 'face' element with an integral list property 'vertex_indices',
    all of whose elements have length 3.
    """
    import plyfile
    plydata = plyfile.PlyData.read(filename)

    x = plydata['vertex']['x']
    y = plydata['vertex']['y']
    z = plydata['vertex']['z']
    V = numpy.vstack((x,y,z)).reshape([3, -1]).transpose()
    #V = numpy.array([x, y, z])
    #print("VERTS:", V.shape)

    vp = []
    for prop in plydata['vertex'].properties:
        vp.append(prop.name)
        print(prop.name)
    
    N = None
    if 'nx' in vp and 'ny' in vp and 'nz' in vp:
        nx = plydata['vertex']['nx']
        ny = plydata['vertex']['ny']
        nz = plydata['vertex']['nz']
        N = numpy.vstack((nx,ny,nz)).reshape([3, -1]).transpose()
    
    T = None
    if 's' in vp and 't' in vp:
        s = plydata['vertex']['s']
        t = plydata['vertex']['t']
        T = numpy.vstack((s,t)).reshape([2, -1]).transpose()

    C = _get_PLY_colours(plydata['vertex'])

    if 'face' in plydata:
        #print(plydata['face'])
        #Face colours?
        if C is None:
            C = _get_PLY_colours(plydata['face'])

        tri_idx = plydata['face']['vertex_indices']
        idx_dtype = tri_idx[0].dtype
        tri_idx = numpy.asarray(tri_idx).flatten()
        #print(type(tri_idx),idx_dtype,tri_idx.shape)
        #print(tri_idx)

        triangles = numpy.array([t.tolist() for t in tri_idx]).flatten()
        #triangles = numpy.fromiter(tri_idx, [('data', idx_dtype, (3,))], count=len(tri_idx))['data']

        return lv.triangles(vertices=V, indices=triangles, colours=C, normals=N, texcoords=T)
    else:
        return lv.points(vertices=V, colours=C, normals=N, texcoords=T)

