#ifndef DISABLE_SERVER
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "GLuciferServer.h"

#include "base64.h"
#include "jpeg/jpge.h"

GLuciferServer* GLuciferServer::_self = NULL; //Static

GLuciferServer* GLuciferServer::Instance(OpenGLViewer* viewer, std::string htmlpath, int port, int quality, int threads)
{
   if (!_self)   // Only allow one instance of class to be generated.
      _self = new GLuciferServer(viewer, htmlpath, port, quality, threads);

   return _self;
}

GLuciferServer::GLuciferServer(OpenGLViewer* viewer, std::string htmlpath, int port, int quality, int threads)
 : viewer(viewer), quality(quality), port(port), threads(threads), path(htmlpath)
{
   jpeg = NULL;
   updated = false;
   client_id = 0;
   // Initialize mutex and condition variable objects
   pthread_mutex_init(&cs_mutex, NULL);
   pthread_mutex_init(&viewer->cmd_mutex, NULL);
   pthread_cond_init (&condition_var, NULL);
}

GLuciferServer::~GLuciferServer()
{
   pthread_cond_broadcast(&condition_var);  //Display complete signal
   if (ctx)
      mg_stop(ctx);
}

// virtual functions for window management
void GLuciferServer::open(int width, int height)
{
   //Enable the animation timer
   //viewer->animate(250);   //1/4 sec timer

   char ports[16], threadstr[16];
   sprintf(ports, "%d", port);
   sprintf(threadstr, "%d", threads);
   debug_print("html path: %s ports: %s\n", path.c_str(), ports);
   const char *options[] = {
     "document_root", path.c_str(),
     "listening_ports", ports,
     "num_threads", threadstr,
     NULL
   };

   ctx = mg_start(&GLuciferServer::callback, NULL, options);
}

void GLuciferServer::resize(int new_width, int new_height)
{
}

void GLuciferServer::display()
{
   //If not currently sending an image, update the image data
   if (pthread_mutex_trylock(&cs_mutex) == 0)
   {
      //CRITICAL SECTION
      size_t size = viewer->width * viewer->height * 3;
      GLubyte *image = new GLubyte[size];
      // Read the pixels (flipped)
      viewer->pixels(image, false, true);

      // Writes JPEG image to memory buffer. 
      // On entry, jpeg_bytes is the size of the output buffer pointed at by jpeg, which should be at least ~1024 bytes. 
      // If return value is true, jpeg_bytes will be set to the size of the compressed data.
      jpeg_bytes = viewer->width * viewer->height * 3;
      if (jpeg) delete[] jpeg;
      jpeg = new unsigned char[jpeg_bytes];

      // Fill in the compression parameter structure.
      jpge::params params;
      params.m_quality = quality;
      params.m_subsampling = jpge::H1V1;   //H2V2/H2V1/H1V1-none/0-grayscale

      if (compress_image_to_jpeg_file_in_memory(jpeg, jpeg_bytes, viewer->width, viewer->height, 3, 
                                                (const unsigned char *)image, params))
      {
         debug_print("JPEG compressed, size %d\n", jpeg_bytes);
      }
      else
         debug_print("JPEG compress error\n");

      delete[] image;

      updated = true; //Set new frame rendered flag
        std::map<int,bool>::iterator iter;
        for (iter = synched.begin(); iter != synched.end(); ++iter)
        {
           iter->second = false; //Flag update waiting
        }
        pthread_cond_broadcast(&condition_var);  //Display complete signal to all waiting clients
      //pthread_cond_signal(&condition_var);  //Display complete signal
      pthread_mutex_unlock(&cs_mutex); //END CRITICAL SECTION
   }
   else
   {
      viewer->postdisplay = true;  //Need to update 
      debug_print("DELAYING IMAGE UPDATE\n");
   }
}

void GLuciferServer::close()
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

void* GLuciferServer::callback(enum mg_event event,
                       struct mg_connection *conn,
                       const struct mg_request_info *request_info)
{
  if (event == MG_NEW_REQUEST)
  {

      debug_print("SERVER REQUEST: %s\n", request_info->uri);

      if (strcmp("/", request_info->uri) == 0)
      {
        mg_printf(conn, "HTTP/1.1 302 Found\r\n"
            "Set-Cookie: original_url=%s\r\n"
            "Location: %s\r\n\r\n",
            request_info->uri, "/index.html?server");
      }
      else if (strstr(request_info->uri, "/timestep=") != NULL)
      {
         //Update timesteps from database, 
         //Load timestep
         //Write image for each window
         int ts = atoi(request_info->uri+10);
         printf("TIMESTEP REQUEST %d\n", ts);
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
         pthread_cond_broadcast(&_self->condition_var);  //Display complete signal
      }
      else if (strstr(request_info->uri, "/objects") != NULL)
      {

         mg_printf(conn, "HTTP/1.1 200 OK\r\n"
              //"Content-Type: application/x-javascript\r\n\r\n"
              //"parseObjects('[");
              "Content-Type: text/plain\r\n\r\n");
         std::string objects = _self->viewer->app->requestData("objects");
         mg_write(conn, objects.c_str(), objects.length());
      }
      else if (strstr(request_info->uri, "/image=") != NULL)
      {
         //Image update requested, wait until data available then send
         int id = atoi(request_info->uri+7);
         pthread_t tid;
         tid = pthread_self();
         pthread_mutex_lock(&_self->cs_mutex);
         debug_print("CLIENT %d THREAD ID %u ENTERING WAIT STATE (synched %d)\n", id, tid, _self->synched[id]);

         //while (!_self->updated && !_self->viewer->quitProgram)
         while (_self->synched[id] && !_self->viewer->quitProgram)
         {
            debug_print("CLIENT %d THREAD ID %u WAITING\n", id, tid);
            _self->viewer->notIdle(1000); //Starts the idle timer (1 second before display fired)
            pthread_cond_wait(&_self->condition_var, &_self->cs_mutex);
         }
         debug_print("CLIENT %d THREAD ID %u RESUMED, quit? %d\n", id, tid, _self->viewer->quitProgram);

         if (!_self->viewer->quitProgram)
         {
            //debug_print("Sending JPEG %d bytes...\n", self->jpeg_bytes);
            mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\n");
            mg_printf(conn, "Content-Length: %d\r\n", _self->jpeg_bytes);
            //Allow cross-origin requests
            mg_printf(conn, "Access-Control-Allow-Origin: *\r\n\r\n");
            //Write raw
            mg_write(conn, _self->jpeg, _self->jpeg_bytes);

            _self->updated = false;
            _self->synched[id] = true;
         }
         pthread_mutex_unlock(&_self->cs_mutex);
      }
      else if (strstr(request_info->uri, "/command=") != NULL)
      {
         mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
         std::string data = request_info->uri+1;
         std::replace(data.begin(), data.end(), ',', '\n');
         const size_t equals = data.find('=');
         const size_t amp = data.find('&');
         if (std::string::npos != equals && std::string::npos != amp)
         {
           //Push command onto queue to be processed in the viewer thread
           pthread_mutex_lock(&_self->viewer->cmd_mutex);
           OpenGLViewer::commands.push_back(data.substr(equals+1, amp-equals-1));
           _self->viewer->postdisplay = true;
           pthread_mutex_unlock(&_self->viewer->cmd_mutex);
         }
      }
      else if (strstr(request_info->uri, "/post") != NULL)
      {
         //mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
         char post_data[32000];
         int post_data_len = mg_read(conn, post_data, sizeof(post_data));
         //printf("%d\n%s\n", post_data_len, post_data);
         if (post_data_len)
         {
           //Push command onto queue to be processed in the viewer thread
           pthread_mutex_lock(&_self->viewer->cmd_mutex);
           OpenGLViewer::commands.push_back(std::string(post_data));
           _self->viewer->postdisplay = true;
           pthread_mutex_unlock(&_self->viewer->cmd_mutex);
         }
      }
      else if (strstr(request_info->uri, "/key=") != NULL)
      {
         mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
         std::string data = request_info->uri+1;
         pthread_mutex_lock(&_self->viewer->cmd_mutex);
         OpenGLViewer::commands.push_back("key " + data);
         _self->viewer->postdisplay = true;
         pthread_mutex_unlock(&_self->viewer->cmd_mutex);
      }
      else if (strstr(request_info->uri, "/mouse=") != NULL)
      {
         mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n"); //Empty response, prevent XML errors
         std::string data = request_info->uri+1;
         pthread_mutex_lock(&_self->viewer->cmd_mutex);
         OpenGLViewer::commands.push_back("mouse " + data);
         _self->viewer->postdisplay = true;
         pthread_mutex_unlock(&_self->viewer->cmd_mutex);
      }
      else
      {
         // No suitable handler found, mark as not processed. Mongoose will
         // try to serve the request.
         return NULL;
      }

     return (void*)request_info;  // Mark as processed
  }
  else
  {
     return NULL;
  }
}

#endif  //DISABLE_SERVER
