# coding: utf-8
lavavu = None
import SimpleHTTPServer
import SocketServer
import urllib

lavavu = None

class LVRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
    def do_GET(self):
        global lavavu
        if self.path.find('image') > 0:
            self.send_response(200)
            self.send_header('Content-type', 'image/png')
            self.end_headers()
            img = lavavu.app.image("", lavavu.res[0], lavavu.res[1])
            img = img[21:] #Strip data:image/png;base64,
            import base64
            self.wfile.write(base64.b64decode(img))
        elif self.path.find('command=') > 0:
            pos1 = self.path.find('=')
            pos2 = self.path.find('?')
            if pos2 < 0: pos2 = len(self.path)
            cmds = urllib.unquote(self.path[pos1+1:pos2])
            #lavavu.parseCommands(cmd)
            lavavu.commands(cmds.split(';'))
            if self.path.find('icommand=') > 0:
                self.send_response(200)
                self.send_header('Content-type', 'image/png')
                self.end_headers()
                img = lavavu.app.image("", lavavu.res[0], lavavu.res[1])
                img = img[21:] #Strip data:image/png;base64,
                import base64
                self.wfile.write(base64.b64decode(img))
            else:
                self.send_response(200)
                self.send_header('Content-type', 'text/plain')
                self.end_headers()
        elif self.path.find('getstate') > 0:
            state = lavavu.getState()
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
    #app.run()
    httpd.serve_forever()

