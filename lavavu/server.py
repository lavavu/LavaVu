import urllib
import os
import threading
import time
import errno
from functools import partial

#Python2/3 compatibility hacks
try:
    from SimpleHTTPServer import SimpleHTTPRequestHandler
except ImportError:
    from http.server import SimpleHTTPRequestHandler

try:
    from SocketServer import TCPServer as HTTPServer
except ImportError:
    from http.server import HTTPServer

try:
    from urllib import unquote
except ImportError:
    from urllib.parse import unquote

_lv = None

"""
HTTP Server interface
"""
class LVRequestHandler(SimpleHTTPRequestHandler, object):

    def __init__(self, viewer, *args, **kwargs):
        #Used with partial() to provide the viewer object
        try:
            self._lv = viewer
            super(LVRequestHandler, self).__init__(*args, **kwargs)
        except (IOError) as e:
            if e.errno == errno.EPIPE:
                # EPIPE error, ignore
                pass
            else:
                raise e

    def serveResponse(self, data, datatype):
        try:
            #Serve provided data, with error check for SIGPIPE (broken connection)
            self.send_response(200)
            self.send_header('Content-type', datatype)
            self.send_header('Access-Control-Allow-Origin', '*')
            #self.send_header('x-colab-notebook-cache-control', 'no-cache') #Colab: disable offline access cache      
            self.end_headers()
            self.wfile.write(data)
        except (IOError) as e:
            if e.errno == errno.EPIPE:
                # EPIPE error, ignore
                pass
            else:
                raise e

    def do_GET(self):
        if self.path.find('image') > 0:
            self.serveResponse(bytearray(self._lv.jpeg()), 'image/jpeg')
        elif self.path.find('command=') > 0:
            pos1 = self.path.find('=')
            pos2 = self.path.find('?')
            if pos2 < 0: pos2 = len(self.path)
            cmds = unquote(self.path[pos1+1:pos2])
            if len(cmds) and cmds[0] == '_':
                #base64 encoded JSON state?
                self._lv.commands(cmds)
            else:
                #Run commands from list
                self._lv.commands(cmds.split(';'))

            #Serve image or just respond 200
            if self.path.find('icommand=') > 0:
                self.serveResponse(bytearray(self._lv.jpeg()), 'image/jpeg')
            else:
                self.serveResponse('', 'text/plain')

        elif self.path.find('getstate') > 0:
            state = self._lv.app.getState()
            self.serveResponse(bytearray(state, 'utf-8'), 'text/plain; charset=utf-8')
        else:
            return SimpleHTTPRequestHandler.do_GET(self)


    #Stifle log output
    def log_message(self, format, *args):
        return

"""
HTTP Server manager class
"""
class Server(threading.Thread):
    def __init__(self, viewer, port=9000, ipv6=False, retries=20):
        self.viewer = viewer
        self.port = port
        self.retries = retries
        self.maxretries = retries
        self.ipv6 = ipv6
        super(Server, self).__init__()

    def run(self):
        httpd = None
        HTTPServer.allow_reuse_address = True
        try:
            # We "partially apply" our first argument to get the viewer object into LVRequestHandler
            handler = partial(LVRequestHandler, self.viewer)
            if self.ipv6:
                import socket
                HTTPServer.address_family = socket.AF_INET6
                httpd = HTTPServer(('::', self.port), handler)
            else:
                httpd = HTTPServer(('0.0.0.0', self.port), handler)

            # Handle requests
            # A timeout is needed for server to check periodically if closing
            httpd.timeout = 1
            while not self.viewer._closing:
                httpd.handle_request()

        except (Exception) as e:
            #Try another port
            if e.errno == errno.EADDRINUSE:
                self.port += 1
                self.retries -= 1
                #print("Port already in use - retry ", (self.maxretries - self.retries), "Port: ", self.port)
                if self.retries < 1:
                    print("Failed to start server, max retries reached")
                    exit(1)
                #Try again
                self.run()
            else:
                print("Server start failed: ",e,e.errno)
                exit(1)

def serve(viewer, port=9000, ipv6=False, retries=20):
    s = Server(viewer, port, ipv6, retries)
    s.daemon = True #Place in background so will be closed on program exit
    s.start()
    return s

"""
Main entry point - run server and open browser interface
"""
if __name__ == '__main__':
    import lavavu
    lv = lavavu.Viewer(port=9000)
    #lv.animate(1) #Required to show viewer window and handle mouse/keyboard events there too
    lv.browser()
    lv._thread.join() #Wait for server to quit

