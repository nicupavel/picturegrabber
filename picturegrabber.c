#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/videodev2.h>

#include <jpeglib.h> 

/*  Set these according to your capture device set-up */
#define VID_DEVICE 	"/dev/video0" /* Device Name */
#define VID_PIX_FLAGS 	V4L2_FMT_FLAG_ODDFIELD /* Video Output Format */
#define VID_STD	  	"NTSC" /* Default Standard */
#define VID_WIDTH  	320 /* Default Width */
#define VID_HEIGHT 	240 /* Default Height */
#define VID_STREAMING 	0 /* Streaming mode boolean */
#define STREAMBUFS	4 /* Number of streaming buffer */
#define DEFAULT_DEPTH	16
#define DEFAULT_FPS	1
/* Device Catpure Objects */
typedef struct tag_vimage
  {
    struct v4l2_buffer vidbuf;
    char *data;
    int   length;
  }
VIMAGE;

typedef struct
{
  int fd_video;
  int width;
  int height;
  int depth;
  int pixelformat;
  VIMAGE vimage[STREAMBUFS];
  struct v4l2_format fmt;
  struct v4l2_requestbuffers req;
}capture_device;

/* Size Menu Objects */
struct
  {
    int width, height;
  }
capturesize[] =
{
  { 160, 120 } ,
  { 192, 144 } ,
  { 320, 240 } ,
  { 384, 288 } ,
  { 640, 480 } ,
  { 768, 576 } ,
};
 
/***************************/
/* Device Catpure Routines */
/***************************/

int capture_fmt (int x_depth)
{
  int fbf = V4L2_PIX_FMT_RGB565;

  if (x_depth == 15)
    fbf = V4L2_PIX_FMT_RGB555;
  else if (x_depth == 16)
    fbf = V4L2_PIX_FMT_RGB565;
  else if (x_depth == 24)
    fbf = V4L2_PIX_FMT_BGR24;
  else if (x_depth == 32)
    fbf = V4L2_PIX_FMT_BGR32;
  else
    {
      printf ("Unrecognized display depth. You may get an "
	      "X Windows error.\n");
    }
    
  return fbf;
}


int
capture_perf (capture_device * pcapt_d)
{
  struct v4l2_performance perf;
 
  if (ioctl (pcapt_d->fd_video, VIDIOC_G_PERF, &perf) != 0)
    printf ("G_PERF returned error code %d\n", errno);
  else
    printf ("Captured %d frames and lost %d frames\n", perf.frames,
	    perf.framesdropped);
  return (perf.frames);
}

void
capture_end (capture_device * pcapt_d)
{
  close (pcapt_d->fd_video);
}

void
capture_std (capture_device * pcapt_d)
{
 int err;
 
// v4l2_std NTSC to Standard
 err = ioctl (pcapt_d->fd_video, VIDIOC_S_STD, 1);
 if (err)
   {
     perror ("S_STD in capture_std");
   }
}

void
capture_size (capture_device * pcapt_d, int width, int height)
{
 int err;

 pcapt_d->fmt.fmt.pix.width = width;
 pcapt_d->fmt.fmt.pix.height = height;
 err = ioctl (pcapt_d->fd_video, VIDIOC_S_FMT, &pcapt_d->fmt);
 if (err)
   {
     perror ("S_FMT in capture_size");
   }
}

void
capture_stop (capture_device * pcapt_d, int streaming)
{
  int i, err;

  if (pcapt_d->fd_video >= 0)
    {
      if (streaming)
        {
          i = V4L2_BUF_TYPE_CAPTURE;
          err = ioctl (pcapt_d->fd_video, VIDIOC_STREAMOFF, &i);
          for (i = 0; i < pcapt_d->req.count; ++i)
  	    {
	      if (pcapt_d->vimage[i].data)
	        munmap (pcapt_d->vimage[i].data, pcapt_d->vimage[i].length);
	      pcapt_d->vimage[i].data = NULL;
	    }
	}
      else
        {
          if (pcapt_d->vimage[0].data)
  	    free (pcapt_d->vimage[0].data);
          pcapt_d->vimage[0].data = NULL;
	}
    }
}

int
capture_start (capture_device * pcapt_d, int streaming)
{
  struct v4l2_standard std;
  int i, err;

  /* Get Video Standard */
  err = ioctl (pcapt_d->fd_video, VIDIOC_G_STD, &std);
  if (err)
    perror ("G_STD in capture_start");

  /* Get Video Format */
  pcapt_d->fmt.type = V4L2_BUF_TYPE_CAPTURE;
  err = ioctl (pcapt_d->fd_video, VIDIOC_G_FMT, &pcapt_d->fmt);
  if (err)
    perror ("G_FMT in capture_start");

  pcapt_d->width = pcapt_d->fmt.fmt.pix.width;
  pcapt_d->height = pcapt_d->fmt.fmt.pix.height;
  pcapt_d->depth = pcapt_d->fmt.fmt.pix.depth;
  pcapt_d->pixelformat = pcapt_d->fmt.fmt.pix.pixelformat;
  
 if (streaming)
   {
     /* Ask Video Device for Buffers */
     pcapt_d->req.count = STREAMBUFS;
     pcapt_d->req.type = V4L2_BUF_TYPE_CAPTURE;
     err = ioctl (pcapt_d->fd_video, VIDIOC_REQBUFS, &pcapt_d->req);
     if (err < 0 || pcapt_d->req.count < 1)
       {
         perror ("REQBUFS in capture _start\n");
         return 0;
       }
    
     /* Query each buffer and map it to the video device */
     for (i = 0; i < pcapt_d->req.count; ++i)
       {
         struct v4l2_buffer *vidbuf = &pcapt_d->vimage[i].vidbuf;
      
         vidbuf->index = i;
         vidbuf->type = V4L2_BUF_TYPE_CAPTURE;
         err = ioctl (pcapt_d->fd_video, VIDIOC_QUERYBUF, vidbuf);
         if (err < 0)
   	   {
	     perror ("QUERYBUF in capture_start");
	     return 0;
	   }
	   
         pcapt_d->vimage[i].length = 0;
         pcapt_d->vimage[i].data = mmap (0, vidbuf->length, 
	           PROT_READ|PROT_WRITE, MAP_SHARED,
	           pcapt_d->fd_video, vidbuf->offset);
         if ((int) pcapt_d->vimage[i].data == -1)
	   {
	     perror ("mmap() in capture_start");
	     return 0;
   	   }
         pcapt_d->vimage[i].length = vidbuf->length; 

	 err = ioctl (pcapt_d->fd_video, VIDIOC_QBUF, vidbuf);
         if (err)
           {
	     perror ("QBUF in capture_start");
	     return 0;
           }
       }
       
     /* Set video stream capture on */
     err = ioctl (pcapt_d->fd_video, VIDIOC_STREAMON, &pcapt_d->vimage[0].vidbuf.type);
     if (err)
       perror ("STREAMON in capture_start");
   }
 else
    {
      /* Alloc one Buffer for one image */
      int sizeimg = pcapt_d->width*pcapt_d->height*pcapt_d->depth/8;
      
      pcapt_d->req.count = 1;
      pcapt_d->vimage[0].data = malloc (sizeimg);
      if (pcapt_d->vimage[0].data == NULL)
        {
          printf ("malloc(%d) failed in capture_start\n", sizeimg);
          return 0;
        }
    }

  return 1;
}


int
capture_init (capture_device * pcapt_d, char *my_device, 
              unsigned int format, int width, int height, int depth)
{
  int err;
  struct v4l2_capability cap;
  struct v4l2_streamparm parm;
  struct v4l2_standard std;

  /* Open Video Device */
  pcapt_d->fd_video = open (my_device, O_RDWR);
  if (pcapt_d->fd_video < 0)
    {
      printf ("No video device \"%s\"\n", my_device);
      return 0;
    }

  /* Querry Video Device Capabilities */
  err = ioctl (pcapt_d->fd_video, VIDIOC_QUERYCAP, &cap);
  if (err)
    {
      perror ("QUERYCAP in capture_init");
      return 0;
    }
  if (cap.type != V4L2_TYPE_CAPTURE)
    {
      printf ("Not a capture device.\n");
      return 0;
    }
  if (!(cap.flags & V4L2_FLAG_READ))
    {
      printf ("Device does not support the read() call.\n");
    }
  if (!(cap.flags & V4L2_FLAG_STREAMING))
    {
      printf ("Device does not support streaming capture.\n");
    }


  /* Set Video Parameters */
  parm.type = V4L2_BUF_TYPE_CAPTURE;
  err = ioctl (pcapt_d->fd_video, VIDIOC_G_PARM, &parm);
  if (err)
    perror ("G_PARM in capture_init");
  
  err = ioctl (pcapt_d->fd_video, VIDIOC_S_PARM, &parm);
  if (err)
    perror ("S_PARM in capture_init");
    
  /* Get Video Standard */
  err = ioctl (pcapt_d->fd_video, VIDIOC_G_STD, &std);
  if (err)
    perror ("G_STD in capture_init");

  /* Set Video Format */
  pcapt_d->fmt.type = V4L2_BUF_TYPE_CAPTURE;
  err = ioctl (pcapt_d->fd_video, VIDIOC_G_FMT, &pcapt_d->fmt);
  if (err)
    perror ("G_FMT in capture_init");

  if (width)  
    pcapt_d->fmt.fmt.pix.width = width;
  else
    pcapt_d->fmt.fmt.pix.width = VID_WIDTH;
  if (height) 
    pcapt_d->fmt.fmt.pix.height = height;
  else
    pcapt_d->fmt.fmt.pix.height = VID_HEIGHT;
  if (depth) 
    pcapt_d->fmt.fmt.pix.depth = depth;
    
  pcapt_d->fmt.fmt.pix.flags |= VID_PIX_FLAGS;
#ifdef VID_PIXELFORMAT
  pcapt_d->fmt.fmt.pix.pixelformat = VID_PIXELFORMAT;
#else
  pcapt_d->fmt.fmt.pix.pixelformat = format;
#endif

  pcapt_d->width = pcapt_d->fmt.fmt.pix.width;
  pcapt_d->height = pcapt_d->fmt.fmt.pix.height;
  pcapt_d->depth = pcapt_d->fmt.fmt.pix.depth;
  pcapt_d->pixelformat = pcapt_d->fmt.fmt.pix.pixelformat;
  
  err = ioctl (pcapt_d->fd_video, VIDIOC_S_FMT, &pcapt_d->fmt);
  if (err)
    perror ("S_FMT in capture_init");
  
  return (1);
}

int
capture_read (capture_device * pcapt_d, int sz_img)
{
  int err;
  
  err = read (pcapt_d->fd_video, pcapt_d->vimage[0].data, sz_img);
  if (err < 0)
    {
      perror ("READ in capture_read");
      return 0;
    }
  
  return 1;
}

int
capture_dq (capture_device * pcapt_d,
	    struct v4l2_buffer *pbuf,
	    int streaming)
{
  int err, n;
  fd_set rdset;
  struct timeval timeout;

  if (streaming)
    {
      FD_ZERO (&rdset);
      FD_SET (pcapt_d->fd_video, &rdset);

      timeout.tv_sec = 1;
      timeout.tv_usec = 0;

      n = select (pcapt_d->fd_video + 1, &rdset, NULL, NULL, &timeout);
      if (n == -1)
	fprintf (stderr, "select error.\n");
      else if (n == 0)
	fprintf (stderr, "select timeout\n");
      else if (FD_ISSET (pcapt_d->fd_video, &rdset))
	{
	  pbuf->type = pcapt_d->vimage[0].vidbuf.type;
	  err = ioctl (pcapt_d->fd_video, VIDIOC_DQBUF, pbuf);
	  if (err)
	    {
	      perror ("DQBUF in capture_dq");
	      return 0;
	    }
	  return 1;
	}
    }
  else
    {
      capture_read(pcapt_d, 1000000);
      pbuf->index = 0;
      return 1;
    }
  return 0;
}

void
capture_q (capture_device * pcapt_d,
	   struct v4l2_buffer *pbuf,
	   int streaming)
{
  int err;

  if (streaming)
    {
      err = ioctl (pcapt_d->fd_video, VIDIOC_QBUF, pbuf);
      if (err)
	perror ("QBUF in capture_q");
    }
}



void grabSetFps(int fd, int fps)
{
  struct v4l2_streamparm params;

  printf("called v4l2_set_fps with fps=%d\n",fps);
  params.type = V4L2_BUF_TYPE_CAPTURE;
  ioctl(fd, VIDIOC_G_PARM, &params);
  printf("time per frame is: %ld\n", params.parm.capture.timeperframe);
  params.parm.capture.capturemode |= V4L2_CAP_TIMEPERFRAME;
  params.parm.capture.timeperframe = 10000000 / fps;
  if (fps == 30)
    params.parm.capture.timeperframe = 333667;
  printf("time per frame is: %ld\n", params.parm.capture.timeperframe);
  ioctl(fd, VIDIOC_S_PARM, &params);
  
  params.parm.capture.timeperframe = 0;
  ioctl(fd, VIDIOC_G_PARM, &params);
  printf("time per frame is: %ld\n", params.parm.capture.timeperframe);  
}

/*****************************************/
/* Export functions                      */
/*****************************************/

int export_jpeg (capture_device * pcapt_d,int count) {

        char *fileout = (char *) malloc (50);
	snprintf (fileout,50,"%s%s","mediabox",".jpg");
	int lx=pcapt_d->width;
	int ly=pcapt_d->height;
	int jsmooth=60;
	int jopt=40;
	int jquality=90;
	int k;
	char *img = (char *) malloc (lx*ly*pcapt_d->depth*sizeof(char));
	img = pcapt_d->vimage[0].data;
	char t;

	for (k = 0; k < 1000000; k += 3)
	{
			t = img[k];
			img[k] = img[k + 2];
			img[k + 2] = t;
	}
	
	FILE *fp;
	unsigned char *line;  // pointer to beggining of line
	unsigned int linesize = lx * 3, i;
	struct jpeg_compress_struct cinfo;
        struct jpeg_error_mgr jerr;

	if (!strncmp(fileout, "-", 1)) {
		fp=stdout; // we dump the file to stdout
	} else {
		if ( (fp=fopen(fileout , "w")) == NULL) {
        		perror ("fopen");
			return -1;
		}
	}
	
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
        jpeg_stdio_dest(&cinfo, fp);
        cinfo.image_width = lx;
        cinfo.image_height = ly;
	cinfo.smoothing_factor = jsmooth;
	cinfo.optimize_coding = jopt;

	cinfo.input_components = 3;
	if (pcapt_d->depth == 1)  cinfo.in_color_space = JCS_GRAYSCALE;
	else cinfo.in_color_space = JCS_RGB;

	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, jquality, TRUE);
	jpeg_start_compress(&cinfo, TRUE);
	
	line=img;

	for (i = 1; i <= ly; i++) {
		jpeg_write_scanlines(&cinfo, &line, 1);
		line=img + (linesize * i);
		}
	
	jpeg_finish_compress(&(cinfo));
	jpeg_destroy_compress(&(cinfo));
	fclose (fp);
	fprintf (stdout, "Done exporting to jpeg\n");
	return (0);
}

int
main (int argc, char *argv[])
{
  capture_device capt_d;

  /* Video Parameters */
  char my_device[64];
  int format,i=0;

  /*    Put in the device node name */
  strcpy (my_device, VID_DEVICE);
  
  format = capture_fmt(DEFAULT_DEPTH);
  capture_init (&capt_d, my_device, format, 0, 0, 0);
  
  grabSetFps(capt_d.fd_video, DEFAULT_FPS);

  if (!capture_start (&capt_d, VID_STREAMING))
    exit(1);
    
  printf ("Capturing %dx%dx%d \"%4.4s\" images\n",
	  capt_d.width, capt_d.height, capt_d.depth,
	  (char *) &(capt_d.pixelformat));
  printf ("Images are %d bytes each\n", 
          capt_d.width * capt_d.height * capt_d.depth / 8);

    for (;;) {
	struct v4l2_buffer tempbuf;
	if (capture_dq (&capt_d, &tempbuf, VID_STREAMING)){
	i++;
	    export_jpeg (&capt_d,i);
  	    capture_q (&capt_d, &tempbuf, VID_STREAMING);
	}
    }
    return 0;
}

