# coding: utf-8
lavavu = None
import SimpleHTTPServer
import SocketServer
import urllib
import base64
import os

lavavu = None

class LVRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
    def serveImage(self):
        start = len('data:image/jpeg;base64,')
        self.send_response(200)
        self.send_header('Content-type', 'image/jpeg')
        self.end_headers()
        img = lavavu.frame()
        img = img[start:]
        self.wfile.write(base64.b64decode(img))

    def do_GET(self):
        global lavavu
        if self.path.find('image') > 0:
            self.serveImage()
        elif self.path.find('command=') > 0:
            pos1 = self.path.find('=')
            pos2 = self.path.find('?')
            if pos2 < 0: pos2 = len(self.path)
            cmds = urllib.unquote(self.path[pos1+1:pos2])
            #Run commands
            lavavu.commands(cmds.split(';'))
            if self.path.find('icommand=') > 0:
                self.serveImage()
            else:
                self.send_response(200)
                self.send_header('Content-type', 'text/plain')
                self.end_headers()
        elif self.path.find('getstate') > 0:
            state = lavavu.app.getState()
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            self.wfile.write(state)
        else:
            return SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)

SocketServer.TCPServer.allow_reuse_address = True
httpd = SocketServer.TCPServer(("0.0.0.0", 9999), LVRequestHandler)

def serve(lv):
    global lavavu
    lavavu = lv
    lv.app.viewer.quitProgram = False
    while not lv.app.viewer.quitProgram:
        httpd.handle_request()

