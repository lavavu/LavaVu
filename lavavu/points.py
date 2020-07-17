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
import convert

def loadpointcloud(filename, subsample=1, dtype=numpy.float32, components=['x', 'y', 'z', 'red', 'green', 'blue', 'alpha']):
    """
    Attempt to load the passed point cloud file based
    Pick loader based on extension

    Parameters
    ----------
    filename, str
        Full path to file to load
    subsample, int
        Subsample factor, sample every Nth point
    dtype, numpy.dtype
        Type of data
    components, list
        List of components per point

    Returns
    -------
    vertices : array
        numpy array x,y,z
    colours : array
        numpy array r,g,b or r,g,b,a
    """
    fn, ext = os.path.splitext(filename)
    ext = ext.lower()
    if ext == '.xyz':
        #Loop over lines in input file
        count = 0
        V = []
        C = []
        with open(filename, 'r') as file:
            for line in file:
                #Subsample?
                count += 1
                if subsample > 1 and count % subsample != 1: continue

                if count % 10000 == 0:
                  print(count)
                  sys.stdout.flush()

                #Read particle position
                data = re.split(r'[,;\s]+', line.rstrip())
                if len(data) < 3: continue
                x = float(data[0])
                y = float(data[1])
                z = float(data[2])

                V.append((x, y, z))

                #R,G,B[,A] colour if provided
                if len(data) >= 7:
                    C.append((int(data[3]), int(data[4]), int(data[5]), int(data[6])))
                elif len(data) == 6:
                    C.append((int(data[3]), int(data[4]), int(data[5]), 255))

        V = numpy.array(V)
        C = numpy.array(C)

    elif ext == '.xyzb':
        inbytes = os.path.getsize(filename)
        #All floats
        #num = inbytes / (4 * 6) # x y z r g b
        #All floats, rgba
        #num = inbytes / (4 * 7) # x y z r g b a
        #All doubles, rgba
        #num = inbytes / (8 * 7) # x y z r g b a
        #Float vert, byte colour
        #num = inbytes / (4 * 3 + 4) # x y z r g b a
        sz = numpy.dtype(dtype).itemsize
        num = inbytes // (sz * len(components))

        with open(filename, 'rb') as infile:
            print("Importing " + str(num//subsample) + " points")
            arr = numpy.fromfile(infile, dtype='float64')
            infile.close()

            arr = arr.astype(dtype).reshape(len(arr)//len(components), len(components))
            if subsample > 1:
                arr = arr[::subsample]

            #Convert float rgba to uint32 colour
            colours = None
            if len(components) > 6:
                colours = arr[:,3:len(components)]
                colours *= 255
                colours = colours.astype('uint8').ravel()
                colours = colours.view(dtype='uint32')

            return (arr[:,0:3], colours)

    elif ext == '.obj':
        print("Loading OBJ")
        import pywavefront
        scene = pywavefront.Wavefront(filename)
        V = numpy.array(scene.vertices)

        #SubSample
        if subsample > 1:
            V = V[::subsample]
            print("Subsampled:",V.shape)

        verts = V[:,0:3]
        colours = V[:,3:] * 255

        #Load positions and RGB
        print("Creating visualisation ")
        return (verts, colours)

    elif ext == '.ply':
        print("Loading PLY")
        from plyfile import PlyData, PlyElement       
        plydata = PlyData.read(filename)
        if plydata:
            x = plydata['vertex']['x']
            y = plydata['vertex']['y']
            z = plydata['vertex']['z']
            V = numpy.vstack((x,y,z)).reshape([3, -1]).transpose()

            C = convert._get_PLY_colours(plydata.elements[0])

            #SubSample
            if subsample > 1:
                V = V[::subsample]
                if C is not None:
                    C = C[::subsample]

            return (V, C)

    elif ext == '.las' or ext == '.laz':
        print("Loading LAS")
        import laspy
        infile = laspy.file.File(filename, mode="r")
        '''
        print(infile)
        print(infile.point_format)
        for spec in infile.point_format:
           print(spec.name)
        print(infile.header)
        print(infile.header.dataformat_id)

        print(infile.header.offset)
        print(infile.header.scale)
        '''

        #Grab all of the points from the file.
        #point_records = infile.points
        #print(point_records)
        #print(point_records.shape)

        #Convert colours from short to uchar
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

