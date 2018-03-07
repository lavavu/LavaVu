#ifndef DISABLE_SERVER
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "Server.h"
#include "Util.h"
#if defined _WIN32
#include <SDL/SDL_syswm.h>
#endif

#include "base64.h"

#define MAX_POST_LEN 32767

Server* Server::_self = NULL; //Static
int Server::refs = 0;

//Defaults
int Server::port = 8080;
int Server::quality = 90;
int Server::threads = 2;
bool Server::render = false;
std::string Server::htmlpath = "html";

Server* Server::Instance(OpenGLViewer* viewer)
{
  if (!_self)   // Only allow one instance of class to be generated.
    _self = new Server(viewer);
  else
    _self->viewer = viewer; //Replace viewer

  //Increment reference count
  refs++;
  //printf("Server Instance Retrieved (%d)\n", refs);
  return _self;
}

void Server::Delete()
{
  if (!_self) return;
  refs--;
  //printf("Server Instance Discarded (%d)\n", refs);
  //Only delete when no more references
  if (refs == 0)
  {
    delete _self;
    _self = NULL;
  }
}

Server::Server(OpenGLViewer* viewer) : viewer(viewer)
{
  imageCache = NULL;
  image_file_data = NULL;
  updated = false;
  client_id = 0;
  synched[0] = true;
  ctx = NULL;
}

Server::~Server()
{
  cv.notify_all(); //Display complete signal
  if (ctx)
    mg_stop(ctx);
}

// virtual functions for window management
void Server::open(int width, int height)
{
  if (!port) return;
  if (ctx)
  {
    std::cerr << "HTTP server already running" << std::endl;
    viewer->port = port;
    return;
  }
  //Enable the timer
  //viewer->setTimer(250);   //1/4 sec timer
  struct mg_callbacks callbacks;

  char ports[16], threadstr[16];
  sprintf(ports, "%d", port);
  sprintf(threadstr, "%d", threads);
  debug_print("html path: %s ports: %s\n", htmlpath.c_str(), ports);
  const char *options[] =
  {
    "document_root", htmlpath.c_str(),
    "listening_ports", ports,
    "num_threads", threadstr,
    NULL
  };

  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.begin_request = &Server::request;
  if ((ctx = mg_start(&callbacks, NULL, options)) == NULL)
  {
    if (port < 9000)
    {
      //Try another port
      port++;
      open(width, height);
    }
    else
    {
      std::cerr << "HTTP server open failed" << std::endl;
    }
    return;
  }

  viewer->port = port;
  std::cerr << "HTTP server running on port " << port << std::endl;
}

void Server::resize(int new_width, int new_height)
{
}

bool Server::compare(ImageData* image)
{
  bool match = false;
  if (imageCache)
  {
    match = true;
    if (image->size() != imageCache->size()) return false;
    for (unsigned int i=0; i<image->size(); i++)
    {
      if (image->pixels[i] != imageCache->pixels[i])
      {
        match = false;
        break;
      }
    }
    delete imageCache;
  }
  imageCache = image;
  return match;
}

void Server::display()
{
  //Image serving can be disabled by global prop
  if (!ctx || !render) return;
  if (quality < 50 || quality > 100) quality = -1;  //Ensure valid, use PNG if not in [50,100] range

  //If not currently sending an image, update the image data
  if (cs_mutex.try_lock())
  {
    //CRITICAL SECTION
    // Read the pixels (flipped)
    ImageData *image = viewer->pixels(NULL, 3); //, true);

    if (!compare(image))
    {
      image_file_data = getImageBytes(image, &image_file_bytes, quality);
      updated = true; //Set new frame rendered flag
      for (auto iter = synched.begin(); iter != synched.end(); ++iter)
        iter->second = false; //Flag update waiting
      cv.notify_all(); //Display complete signal to all waiting clients
    }
    cs_mutex.unlock(); //END CRITICAL SECTION
  }
  else
  {
    viewer->postdisplay = true;  //Need to update
    debug_print("DELAYING IMAGE UPDATE\n");
  }
}

void Server::close()
{
}

void send_file(const char *fname, struct mg_connection *conn)
{
  std::ifstream file(fname, std::ios::binary);
  std::streambuf* raw_buffer = file.rdbuf();

  //size_t len = raw_buffer->pubseekoff(0, std::ios::end, std::ios::in);;
  file.seekg(0,std::ios::end);
  size_t len = file.tellg();
  file.seekg(0,std::ios::beg);

  char* src = new char[len];
  raw_buffer->sgetn(src, len);

  //Base64 encode!
  std::string encoded = base64_encode(reinterpret_cast<const unsigned char*>(src), len);
  mg_write(conn, encoded.c_str(), encoded.length());

  delete[] src;
}

void send_string(std::string str, struct mg_connection *conn)
{
  //Base64 encode!
  std::string encoded = base64_encode(reinterpret_cast<const unsigned char*>(str.c_str()), str.length());
  mg_write(conn, encoded.c_str(), encoded.length());
}

int Server::request(struct mg_connection *conn)
{
  const struct mg_request_info *request_info = mg_get_request_info(conn);
  int id = -1;
  debug_print("SERVER REQUEST: %s\n", request_info->uri);

  //Default location is control interface only
  if (strcmp("/", request_info->uri) == 0)
  {
    mg_printf(conn, "HTTP/1.1 302 Found\r\n"
              "Set-Cookie: original_url=%s\r\n"
              "Location: %s\r\n\r\n",
              request_info->uri, "/index.html?server");
  }
  //Default viewer with server enabled
  if (strcmp("/viewer", request_info->uri) == 0)
  {
    mg_printf(conn, "HTTP/1.1 302 Found\r\n"
              "Set-Cookie: original_url=%s\r\n"
              "Location: %s\r\n\r\n",
              request_info->uri, "/viewer.html?server");
  }
  else if (strstr(request_info->uri, "/timestep=") != NULL)
  {
    //Update timesteps from database,
    //Load timestep
    //Write image for each window
    int ts = atoi(request_info->uri+10);
    debug_print("TIMESTEP REQUEST %d\n", ts);
  }
  else if (strstr(request_info->uri, "/connect") != NULL)
  {
    //Return an id assigned to this client
    _self->client_id++;
    debug_print("NEW CONNECTION: %d (%d THREADS)\n", _self->client_id, _self->threads);
    mg_printf(conn, "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/plain\r\n\r\n"
              "%d", _self->client_id);
    _self->updated = true; //Force update
    _self->synched[_self->client_id] = false;
  }
  else if (strstr(request_info->uri, "/disconnect=") != NULL)
  {
    int id = atoi(request_info->uri+12);
    _self->synched.erase(id);
    debug_print("%d DISCONNECTED, CLIENT %d\n", id, _self->client_id);
    mg_printf(conn, "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/plain\r\n\r\n");
    _self->updated = true; //Force update
    _self->cv.notify_all(); //Display complete signal to all waiting clients
  }
  else if (strstr(request_info->uri, "/objects") != NULL || 
           strstr(request_info->uri, "/getstate") != NULL)
  {
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
    std::string objects = _self->viewer->app->requestData("objects");
    mg_write(conn, objects.c_str(), objects.length());
  }
  else if (strstr(request_info->uri, "/history") != NULL)
  {
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
    std::string history = _self->viewer->app->requestData("history");
    mg_write(conn, history.c_str(), history.length());
  }
  else if (strstr(request_info->uri, "/image") != NULL)
  {
    id = 0; //Default client
    //Get id from url if passed eg: /image=id
    if (strstr(request_info->uri, "/image=") != NULL)
      id = atoi(request_info->uri+7);
  }
  else if (strstr(request_info->uri, "command=") != NULL)
  {
    std::string data = request_info->uri+1;
    const size_t equals = data.find('=');
    const size_t amp = data.find('&');
    //Push command onto queue to be processed in the viewer thread
    std::lock_guard<std::mutex> guard(_self->viewer->cmd_mutex);
    if (amp != std::string::npos)
      _self->viewer->commands.push_back(data.substr(equals+1, amp-equals-1));
    else
      _self->viewer->commands.push_back(data.substr(equals+1));
    debug_print("CMD: %s\n", data.substr(equals+1).c_str());
    _self->viewer->postdisplay = true;
    if (strstr(request_info->uri, "icommand=") != NULL)
      id = 0;  //Image requested with command, use default client id
    else
      mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
  }
  else if (strstr(request_info->uri, "/post") != NULL)
  {
    //mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
    char post_data[MAX_POST_LEN+1] = {0};
    int post_data_len = mg_read(conn, post_data, sizeof(post_data));
    if (post_data_len > MAX_POST_LEN)
    {
      std::cerr << "ERROR! Post data truncated, skipping\n";
      post_data_len = 0;
    }
    //printf("%d\n%s\n", post_data_len, post_data);
    if (post_data_len)
    {
      //Push command onto queue to be processed in the viewer thread
      std::lock_guard<std::mutex> guard(_self->viewer->cmd_mutex);
      //Seems post data string can exceed data length or be missing terminator
      post_data[post_data_len] = 0;
      _self->viewer->commands.push_back(std::string(post_data));
      _self->viewer->postdisplay = true;
    }
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n"); //Empty OK response required
  }
  else if (strstr(request_info->uri, "/key=") != NULL)
  {
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
    std::string data = request_info->uri+1;
    std::lock_guard<std::mutex> guard(_self->viewer->cmd_mutex);
    _self->viewer->commands.push_back("key " + data);
    _self->viewer->postdisplay = true;
  }
  else if (strstr(request_info->uri, "/mouse=") != NULL)
  {
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n"); //Empty response, prevent XML errors
    std::string data = request_info->uri+1;
    std::lock_guard<std::mutex> guard(_self->viewer->cmd_mutex);
    _self->viewer->commands.push_back("mouse " + data);
    _self->viewer->postdisplay = true;
  }
  else if (strstr(request_info->uri, "/render") != NULL)
  {
    //Enable/disable image serving
    Server::render = !Server::render;
    std::cerr << "Image serving  " << (Server::render ? "ENABLED" : "DISABLED") << std::endl;
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n"); //Empty OK response required
  }
  else
  {
    // No suitable handler found, mark as not processed. Mongoose will
    // try to serve the request.
    return 0;
  }

  //Respond with an image frame
  if (id >= 0)
  {
    if (!Server::render)
    {
      Server::render = true;
      _self->updated = true; //Force update
      _self->synched[id] = true;
    }

    //Image update requested, wait until data available then send
    std::thread::id tid = std::this_thread::get_id();
    std::unique_lock<std::mutex> lk(_self->cs_mutex);
    debug_print("CLIENT %d THREAD ID %u ENTERING WAIT STATE (synched %d)\n", id, tid, _self->synched[id]);
    _self->cv.wait(lk, [&]{return !_self->synched[id] || _self->viewer->quitProgram;});
    debug_print("CLIENT %d THREAD ID %u RESUMED, quit? %d\n", id, tid, _self->viewer->quitProgram);

    if (!_self->viewer->quitProgram)
    {
      //debug_print("Sending image %d bytes...\n", _self->image_file_bytes);
      if (quality > 0)
        mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\n");
      else
        mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: image/png\r\n");
      mg_printf(conn, "Content-Length: %d\r\n", _self->image_file_bytes);
      //Allow cross-origin requests
      mg_printf(conn, "Access-Control-Allow-Origin: *\r\n\r\n");
      //Write raw
      mg_write(conn, _self->image_file_data, _self->image_file_bytes);

      _self->updated = false;
      _self->synched[id] = true;
    }
  }

  // Returning non-zero tells mongoose that our function has replied to
  // the client, and mongoose should not send client any more data.
  return 1;
}

#endif  //DISABLE_SERVER
