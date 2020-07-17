import urllib
import os
import time
import errno
import weakref
import base64
import json
import socket

#ASYNC
import asyncio
import aiohttp
from aiohttp import web

from urllib.parse import unquote

"""
HTTP Server interface
"""

def _get_viewer(ref):
    #Get from weak reference, if deleted raise exception
    lv = ref()
    if not lv:
        raise(Exception("Viewer not found"))
    return lv

def _execute(lv, cmds):
    if len(cmds) and cmds[0] == '_':
        #base64 encoded commands or JSON state
        cmds = str(base64.b64decode(cmds).decode('utf-8'))
        #cmds = str(base64.b64decode(cmds), 'utf-8')

    #Object to select can be provided in preceding angle brackets
    selobj = None
    if cmds[0] == '<':
        pos = cmds.find('>')
        selobj = lv.objects[cmds[1:pos]]
        cmds = cmds[pos+1:]

    #Execute commands via python API by preceding with '.'
    done = False
    if cmds[0] == '.':
        attr = cmds.split()[0][1:]
        pos = cmds.find(' ')
        params = cmds[pos+1:]
        if selobj:
            #Call on Object
            func = getattr(selobj, attr)
            if func and callable(func):
                func(params)
                done = True
        else:
            #Call on Viewer
            func = getattr(lv, attr)
            if func and callable(func):
                func(params)
                done = True
    elif cmds[0] == '$':
        #Requests prefixed by '$' are sent
        #from property collection controls
        #format is $ID KEY VALUE
        # - ID is the python id() of the properties object
        #    All properties collections are stored on their parent
        #    object using this id in the _collections dict
        # - KEY is the property name key to set
        # - VALUE is a json string containing the value to set
        S = cmds.split()
        target = S[0][1:]
        if target in lv._collections:
            #Get from _collections by id (weakref)
            props = lv._collections[target]()
            props[S[1]] = json.loads(S[2])
            #Check for callback - if provided, call with updated props
            func = getattr(props, 'callback')
            if func and callable(func):
                func(props)

    #Default, call via lv.commands() scripting API
    if not done:
        if selobj:
            selobj.select()
        lv.commands(cmds)

headers = {'Access-Control-Allow-Origin' : '*',
           'x-colab-notebook-cache-control' : 'no-cache'} #Colab: disable offline access cache      

def img_response(lv, query={}):
    global headers
    resp = None
    if 'width' in query and 'height' in query:
        resp = lv.jpeg(resolution=(int(query['width']), int(query['height'])))
    elif 'width' in query:
        resp = lv.jpeg(resolution=(int(query['width']), 0))
    else:
        resp = lv.jpeg()

    #Ensure the response is valid before serving
    if resp is not None:
        return web.Response(body=resp, content_type='image/jpeg', headers=headers)
    else:
        return web.Response(text='', headers=headers)

async def index(request):
    global headers
    #Index request returns full screen interactive view
    lv = _get_viewer(request.app['viewer'])
    w = lv.control.Window(align=None, wrapper=None)
    code = lv.control.show(True, filename="")
    return web.Response(text=code, headers=headers, content_type='text/html')

async def handle_get(request):
    global headers
    lv = _get_viewer(request.app['viewer'])

    #Default to empty OK 200 response
    response = web.Response(text='', headers=headers)

    #print(request.path)
    #for q in request.query:
    #    print(q, request.query[q])

    if request.path.find('image') > 0:
        response = img_response(lv, request.query)

    elif request.path.find('command=') > 0:
        pos1 = request.path.find('=')
        pos2 = request.path.find('?')
        if pos2 < 0: pos2 = len(request.path)
        cmds = unquote(request.path[pos1+1:pos2])

        #Run viewer commands
        _execute(lv, cmds)

        #Serve image or just respond 200
        if request.path.find('icommand=') > 0:
            response = img_response(lv, request.query)

    elif request.path.find('getstate') > 0:
        state = lv.app.getState()
        response = web.Response(text=state, headers=headers, content_type='application/json')
    elif request.path.find('connect') > 0:
        if 'url' in request.query:
            #Save first valid connection URL on the viewer
            url = request.query['url']
            if len(lv._url) == 0:
                lv._url = url
        uid = id(lv)
        response = web.Response(text=str(uid), headers=headers)
    elif request.path.find('key=') > 0:
        pos2 = request.path.find('&')
        cmds = unquote(request.path[1:pos2])
        lv.commands('key ' + cmds, True)
    elif request.path.find('mouse=') > 0:
        pos2 = request.path.find('&')
        cmds = unquote(request.path[1:pos2])
        lv.commands('mouse ' + cmds, True)
    else:
        #Serve other urls as files if available
        #print("UNKNOWN - ", request.path)
        path = request.path
        if os.path.exists(path):
            #OK to always serve files in cwd?
            response = web.FileResponse(path)
        else:
            #Serve files from lavavu html dir
            #print(' - not found in cwd')
            if path[0] == '/': path = path[1:]
            path2 = os.path.join(lv.htmlpath, path)
            if os.path.exists(path2) and os.path.isfile(path2):
                #print(' - found in htmlpath')
                response = web.FileResponse(path2)

    return response

async def index_post(request):
    lv = _get_viewer(request.app['viewer'])
    #print("POST", request.path)
    #text = await request.text()
    text = ''
    #Always interpret post data as commands
    #(can perform othe r actions too based on path later if we want)
    if request.body_exists:
        body = await request.read()
        #body = await request.text()

        cmds = str(body, 'utf-8') #python3 only
        #from urllib.parse import unquote
        #data_string = unquote(body)
        #cmds = str(data_string.decode('utf-8'))

        #Run viewer commands
        _execute(lv, cmds)

    return web.Response(text='', headers=headers)


"""
HTTP Server manager class
"""
class Server(object):
    def __init__(self, viewer, port=8080, ipv6=False, retries=100):
        #Allows viewer to be garbage collected
        self.viewer = weakref.ref(viewer)
        self.ipv6 = ipv6
        self.retries = retries
        self._closing = False
        self._lock = asyncio.Lock()

        #Get port/socket before running server in synchronous code
        self.socket, self.port = _listen(port, self.ipv6, self.retries)

    async def run(self):
        #Server run!
        #Lock until the port is retrieved
        async with self._lock:
            await _serve(self.viewer, self.socket)

        #ONLY NEED THIS IF NO LOOP EXISTS AND WE ARE MANAGING OUR OWN
        #while not self._closing:
        #    await asyncio.sleep(3600)  # sleep forever
        #    #await asyncio.sleep(1)  # sleep forever
        #    print('_', end='')

        #To stop, call cleanup (TODO: put this somewhere in closedown code)
        #await runner.cleanup()

def _listen(port, ipv6, retries):
    #Get port/socket before running server in synchronous code
    #avoids race conditions over port number with subsequent code that 
    #tries to use server.port before it is confirmed/opened
    hostidx = 0
    for i in range(retries):
        try:
            hosts = []
            socktype = socket.AF_INET
            if ipv6:
                hosts = ['::', 'localhost', '::1']
                host = hosts[hostidx]
                socktype = socket.AF_INET6
            else:
                hosts = ['0.0.0.0', 'localhost', '127.0.0.1']
                host = hosts[hostidx]

            #https://github.com/aio-libs/aiohttp/issues/1987#issuecomment-309401600
            sock = socket.socket(socktype)
            sock.bind((host, port))
            # Aiohttp will call 'listen' inside.
            # But it must be called before we actually use the port,
            # any attempts to connect before the 'listen' call will
            # be rejected.
            sock.listen(128)
            params = sock.getsockname()
            port = params[1]
            #print("Socket ready on host %s port %s" % (host, port))
            return sock, port

        except (Exception) as e:
            #Try another port
            if e.errno == errno.EADDRINUSE: #98
                port += 1
                #Try again...
            elif e.errno == errno.EAFNOSUPPORT: #97 : Address family not supported by protocol
                #Try next host name/address
                hostidx += 1
                if hostidx > 2:
                    #Try again without ipv6?
                    if ipv6:
                        ipv6 = False
                    else:
                        ipv6 = True
                    hostidx = 0
                #Try again...
            else:
                print("Socket open failed: ", e, e.errno, host, port)

    print("Failed to open socket, max retries reached")
    return None, 0

async def _serve(viewer, sock):
    try:
        #Create web application manager
        app = web.Application()
        #Store viewer
        app["viewer"] = viewer
        #Add routes
        app.router.add_get('/', index)
        app.router.add_post('/', index_post),
        app.router.add_get('/{tail:.*}', handle_get)

        #Static routes? https://docs.aiohttp.org/en/stable/web_advanced.html
        #app.add_routes([web.static('/', path_to_static_folder)])
        #routes.static('/prefix', path_to_static_folder)

        #app.add_routes([web.get('/', handler),
        #        web.post('/post', post_handler),
        #        web.put('/put', put_handler)])

                #Can't use this, is blocking
                #web.run_app(app, sock=sock)

        # set up aiohttp - like run_app, but non-blocking - socket provided version
        runner = aiohttp.web.AppRunner(app, access_log=None)
        await runner.setup()
        site = aiohttp.web_runner.SockSite(runner, sock=sock)
        await site.start()

        """
        #This works but have to wait for the port to be allocated before using
        # passing socket as above gets port in synchronous code

        # set up aiohttp - like run_app, but non-blocking
        runner = aiohttp.web.AppRunner(app)
        await runner.setup()
        site = aiohttp.web.TCPSite(runner, host=host, port=port, reuse_address=False) 
        await site.start()

        #Get port from first entry in list of active connections
        for s in runner.sites:
            #print(s.name, s._port)
            return s._port #Get actual port allocated
        return 0
        """

    except (Exception) as e:
        print("Server start failed: ", e)

#Ignore SIGPIPE altogether (does not apply on windows)
#import sys
#if sys.platform != 'win32':
#    from signal import signal, SIGPIPE, SIG_IGN
#    signal(SIGPIPE, SIG_IGN)

def serve(viewer, port=None, ipv6=False, retries=100):
    s = Server(viewer, port, ipv6, retries)
    #Attach to event loop
    loop = asyncio.get_event_loop()
    loop.create_task(s.run())
    return s

"""
Main entry point - run server and open browser interface
"""
if __name__ == '__main__':
    import lavavu
    import asyncio
    lv = lavavu.Viewer()
    print(lv.server.port)
    lv.browser()
    lv.app.loop()
    #lv.interactive()

