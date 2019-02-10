"""
Warning! EXPERIMENTAL:
 these features and functions are under development, will have bugs,
 and may be heavily modified in the future

Tools for importing and working with point clouds
- OBJ loader, requires pywavefront
- PLY loader, requires plyfile
- LAS loader, requires laspy

These functions ported from my viz script repo here:
https://github.com/mivp/vizscripts/tree/master/pointcloud
More tools can be found there, including a ptx reader which handles transformations of multiple point clouds into one coord space

"""
import numpy
import os

def loadpointcloud(filename, subsample=1):
    """
    Attempt to load the passed point cloud file based
    Pick loader based on extension

    Returns:
    --------
    tuple: (vertices, colours)
        vertices: numpy array x,y,z
        colours: numpy array r,g,b or r,g,b,a
    """
    fn, ext = os.path.splitext(filename)
    ext = ext.lower()
    if ext == '.obj':
        print("Loading OBJ")
        import pywavefront
        scene = pywavefront.Wavefront(filename)
        V = numpy.array(scene.vertices)
        print(V.shape)

        #SubSample
        if subsample > 1:
            V = V[::ss]
            print("Subsampled:",V.shape)

        verts = V[:,0:3]
        colours = V[:,3:] * 255
        print(verts.shape,colours.shape)

        #Load positions and RGB
        print("Creating visualisation ")
        return (verts, colours)

    elif ext == '.ply':
        print("Loading PLY")
        from plyfile import PlyData, PlyElement       
        plydata = PlyData.read(filename)

        if plydata:
            #SubSample
            #if subsample > 1:
            #    V = V[::ss]
            #   print(V.shape)

            #print("NAME:",plydata.elements[0].name)
            #print("PROPS:",plydata.elements[0].properties)
            #print("DATA:",plydata.elements[0].data[0])
            #print(plydata.elements[0].data['x'])
            #print(plydata['face'].data['vertex_indices'][0])
            #print(plydata['vertex']['x'])
            #print(plydata['vertex'][0])
            #print(plydata.elements[0].ply_property('x'))

            rl = None
            gl = None
            bl = None
            al = None
            for prop in plydata.elements[0].properties:
                #print(prop,prop.name,prop.dtype)
                if 'red' in prop.name: rl = prop.name
                if 'green' in prop.name: gl = prop.name
                if 'blue' in prop.name: bl = prop.name
                if 'alpha' in prop.name: al = prop.name

            x = plydata['vertex']['x']
            y = plydata['vertex']['y']
            z = plydata['vertex']['z']

            if rl and gl and bl:
                r = plydata['vertex'][rl]
                g = plydata['vertex'][gl]
                b = plydata['vertex'][bl]
                if al:
                    a = plydata['vertex'][al]
                    return (numpy.array([x, y, z]), numpy.array([r, g, b, a]))
                else:
                    return (numpy.array([x, y, z]), numpy.array([r, g, b]))
            else:
                return (numpy.array([x, y, z]), None)

    elif ext == '.las':
        print("Loading LAS")
        import laspy
        infile = laspy.file.File(filename, mode="r")
        print(infile)
        print(infile.point_format)
        for spec in infile.point_format:
           print(spec.name)
        print(infile.header)
        print(infile.header.dataformat_id)

        print(infile.header.offset)
        print(infile.header.scale)

        #Grab all of the points from the file.
        #point_records = infile.points
        #print(point_records)
        #print(point_records.shape)

        #Convert colours from short to uchar
        print(infile.red.dtype)
        if infile.red.dtype == numpy.uint16:
            R = (infile.red / 255).astype(numpy.uint8)
            G = (infile.green / 255).astype(numpy.uint8)
            B = (infile.blue / 255).astype(numpy.uint8)
        else:
            R = infile.red
            G = infile.green
            B = infile.blue

        ss = subsample
        if ss > 1:
            V = numpy.array([infile.x[::ss], infile.y[::ss], infile.z[::ss]])
            C = numpy.array([R[::ss],G[::ss],B[::ss]])
            return (V, C)
        else:
            V = numpy.array([infile.x, infile.y, infile.z])
            C = numpy.array([R,G,B])
            return (V, C)

    else:
        print("Unknown point cloud format, extension: ", ext)

