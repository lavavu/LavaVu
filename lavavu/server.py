import urllib
import os
import threading
import time
import errno
from functools import partial
import weakref
import base64
import json

#Python2/3 compatibility hacks
try:
    # Python 2.x
    from SocketServer import ThreadingMixIn
    from SimpleHTTPServer import SimpleHTTPRequestHandler
    #from SimpleHTTPServer import HTTPServer
    from SocketServer import TCPServer as HTTPServer
    from urllib import unquote
except ImportError:
    # Python 3.x
    from socketserver import ThreadingMixIn
    from http.server import SimpleHTTPRequestHandler, HTTPServer
    from urllib.parse import unquote

"""
HTTP Server interface
"""
class LVRequestHandler(SimpleHTTPRequestHandler, object):

    def __init__(self, viewer_weakref, *args, **kwargs):
        #Used with partial() to provide the viewer object
        try:
            self._lv = viewer_weakref
            super(LVRequestHandler, self).__init__(*args, **kwargs)
        except (IOError) as e:
            pass #Just ignore IO errors on server
            if e.errno == errno.EPIPE:
                # EPIPE error, ignore
                pass
            elif e.errno == errno.EPROTOTYPE:
                # MacOS "Protocol wrong type for socket" error, ignore
                pass
            else:
                raise e

    def serveResponse(self, data, datatype):
        try:
            #Serve provided data, with error check for SIGPIPE (broken connection)
            self.send_response(200)
            self.send_header('Content-type', datatype)
            self.send_header('Access-Control-Allow-Origin', '*')
            self.send_header('x-colab-notebook-cache-control', 'no-cache') #Colab: disable offline access cache      
            self.end_headers()
            if data:
                self.wfile.write(data)
        except (IOError) as e:
            if e.errno == errno.EPIPE:
                # EPIPE error, ignore
                pass
            else:
                raise e

    def do_HEAD(self):
        self.serveResponse(None, 'text/html')

    def do_POST(self):
        #Always interpret post data as commands
        #(can perform other actions too based on self.path later if we want)
        data_string = self.rfile.read(int(self.headers['Content-Length']))
        self.serveResponse(b'', 'text/plain')
        #cmds = str(data_string, 'utf-8') #python3 only
        cmds = str(data_string.decode('utf-8'))
        #Run viewer commands
        self._execute(cmds)

    def do_GET(self):
        lv = self._get_viewer()

        if self.path.find('image') > 0:
            self.serveResponse(lv.jpeg(), 'image/jpeg')

        elif self.path.find('command=') > 0:
            pos1 = self.path.find('=')
            pos2 = self.path.find('?')
            if pos2 < 0: pos2 = len(self.path)
            cmds = unquote(self.path[pos1+1:pos2])

            #Run viewer commands
            self._execute(cmds)

            #Serve image or just respond 200
            if self.path.find('icommand=') > 0:
                self.serveResponse(lv.jpeg(), 'image/jpeg')
            else:
                self.serveResponse(b'', 'text/plain')

        elif self.path.find('getstate') > 0:
            state = lv.app.getState()
            self.serveResponse(bytearray(state, 'utf-8'), 'text/plain; charset=utf-8')
            #self.serveResponse(bytearray(state, 'utf-8'), 'text/plain')
        elif self.path.find('connect') > 0:
            uid = id(lv)
            self.serveResponse(bytearray(str(uid), 'utf-8'), 'text/plain; charset=utf-8')
        elif self.path.find('key=') > 0:
            pos2 = self.path.find('&')
            cmds = unquote(self.path[1:pos2])
            lv.commands('key ' + cmds, True)
            self.serveResponse(b'', 'text/plain')
        elif self.path.find('mouse=') > 0:
            pos2 = self.path.find('&')
            cmds = unquote(self.path[1:pos2])
            lv.commands('mouse ' + cmds, True)
            self.serveResponse(b'', 'text/plain')
        else:
            return SimpleHTTPRequestHandler.do_GET(self)

    #Serve files from lavavu html dir
    def translate_path(self, path):
        lv = self._get_viewer()
        if not os.path.exists(path):
            #print(' - not found in cwd')
            if path[0] == '/': path = path[1:]
            path = os.path.join(lv.htmlpath, path)
            if os.path.exists(path) and os.path.isfile(path):
                #print(' - found in htmlpath')
                return path
            else:
                #print(' - not found in htmlpath')
                return SimpleHTTPRequestHandler.translate_path(self, self.path)
        else:
            return SimpleHTTPRequestHandler.translate_path(self, path)

    #Stifle log output
    def log_message(self, format, *args):
        return

    def _get_viewer(self):
        #Get from weak reference, if deleted raise exception
        lv = self._lv()
        if not lv:
            self._closing = True
            raise(Exception("Viewer not found"))
        return lv

    def _execute(self, cmds):
        lv = self._get_viewer()

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

#Optional thread per request version:
class ThreadingHTTPServer(ThreadingMixIn, HTTPServer):
    pass

"""
HTTP Server manager class
"""
class Server(threading.Thread):
    def __init__(self, viewer, port=None, ipv6=False, retries=100):
        if port is None: port = 8080
        self._closing = False
        #Allow viewer to be garbage collected
        self.viewer = weakref.ref(viewer)
        self.port = port
        self.retries = retries
        self.maxretries = retries
        self.ipv6 = ipv6
        super(Server, self).__init__()
        self.daemon = True #Place in background so will be closed on program exit
        self._cv = threading.Condition()

    def handle(self):
        httpd.handle_request()

    def run(self):
        httpd = None
        HTTPServer.allow_reuse_address = False
        try:
            # We "partially apply" our first argument to get the viewer object into LVRequestHandler
            handler = partial(LVRequestHandler, self.viewer)
            if self.ipv6:
                import socket
                HTTPServer.address_family = socket.AF_INET6
                httpd = HTTPServer(('::', self.port), handler)
                #httpd = ThreadingHTTPServer(('::', self.port), handler)
            else:
                httpd = HTTPServer(('0.0.0.0', self.port), handler)
                #httpd = ThreadingHTTPServer(('0.0.0.0', self.port), handler)

            #Sync with starting thread here to ensure server thread has initialised before it continues
            with self._cv:
                self._cv.notifyAll()

            # Handle requests
            #print("Using port: ", self.port)
            # A timeout is needed for server to check periodically if closing
            httpd.timeout = 0.05 #50 millisecond timeout
            while self.viewer() is not None and not self._closing:
                httpd.handle_request()

        except (Exception) as e:
            #Try another port
            if e.errno == errno.EADDRINUSE:
                self.port += 1
                self.retries -= 1
                #print("Port already in use - retry ", (self.maxretries - self.retries), "Port: ", self.port)
                if self.retries < 1:
                    print("Failed to start server, max retries reached")
                    return
                #Try again
                self.run()
            else:
                print("Server start failed: ",e, e.errno, self.port)

def serve(viewer, port=None, ipv6=False, retries=100):
    s = Server(viewer, port, ipv6, retries)
    #Start the thread and wait for it to finish initialising
    with s._cv:
        s.start()
        s._cv.wait()
    return s

#Ignore SIGPIPE altogether (does not work on windows)
#from signal import signal, SIGPIPE, SIG_IGN
#signal(SIGPIPE, SIG_IGN)

"""
Main entry point - run server and open browser interface
"""
if __name__ == '__main__':
    import lavavu
    lv = lavavu.Viewer()
    #lv.animate(1) #Required to show viewer window and handle mouse/keyboard events there too
    lv.browser()
    lv._thread.join() #Wait for server to quit

