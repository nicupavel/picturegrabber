#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <linux/types.h>
#include <errno.h>
#include <signal.h>

#ifdef OLD_V4L
#include <linux/videodev.h>
#else
#include <libv4l1-videodev.h>
#endif

#include <jpeglib.h>

#define TIMEOUT_LEN 1000
#define MAX_RGBA_IMAGE_SIZE ((704 * 576 * 3) + 256)
#define VERSION "0.4.2"

int dev;
int bytes_per_rgb;

struct video_window vid_win;
struct video_capability vid_caps;
struct video_picture vid_pic;
struct video_tuner vid_tun;

unsigned char *grab_data;
int frozen, sequential = 0;
unsigned int timeoutid;
int jsmooth = 0, jopt = 0, jquality = 60;

int export_jpeg(char *filename);
void swap_rgb24(unsigned char *data);

void _sighandler(int sig)
{
  switch (sig)
  {
  case SIGINT: /* ctrl+c */
    fprintf(stderr, "Caught SIGINT - Cleaning \n");
    exit(0);
    break;
  }
}

void usage()
{
  printf("picturegrabber: version %s iTuner Networks Corporation 2003\n", VERSION);
  printf("Usage: picturegrabber [OPTION]\n");
  printf("\t-c <num>  number of shots, -1=unlimited (option MANDATORY)\n");
  printf("\t-i <sec>  interval between shots in seconds (default=4)\n");
  printf("\t-d <dev>  device to open (default=/dev/video)\n");
  printf("\t-D <dir>  output in this directory\n");
  printf("\t-q        qcif 176x144 output (defaults to cif 352x288)\n");
  printf("\t-s        use sequential filename numbering (out1,out2...)\n");
  printf("\t-f <str>  filename prefix (used with -s, default=out)\n");
  printf("\t-v        be verbose\n");
  printf("\t-b        run in background as a daemon\n");
  printf("\t-O        coding optimization  (default=no)\n");
  printf("\t-Q <num>  quality factor [0-100] (default=60)\n");
  printf("\t-S <num>  smoothing factor [0-100] (default=0)\n");
  printf("\t-l        link last saved file as current.jpg \n");
}
int grab()
{
  ssize_t len = 0, size = 0;
  errno = 0;

  while ((len = read(dev, grab_data, MAX_RGBA_IMAGE_SIZE)) <= 0)
  {
    if (errno != 0 && errno != EINTR)
    {
      perror("Error read image\n");
      free(grab_data);
      return (0);
    }
  }
  size += len;
  return (1);
}

int grab_mmap()
{
  grab_data = mmap(0, MAX_RGBA_IMAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, dev, 0);
  printf("mmap().\n");
  return (1);
}

void setbrightness(unsigned int value)
{
  vid_pic.brightness = (value)*256;
  if (ioctl(dev, VIDIOCSPICT, &vid_pic) == -1)
  {
    perror("ioctl (VIDIOCSPICT)");
    return;
  }
}

void setcolor(unsigned int value)
{
  vid_pic.colour = (value)*256;
  if (ioctl(dev, VIDIOCSPICT, &vid_pic) == -1)
  {
    perror("ioctl (VIDIOCSPICT)");
    return;
  }
}

void setcontrast(unsigned int value)
{
  vid_pic.contrast = (value)*256;
  if (ioctl(dev, VIDIOCSPICT, &vid_pic) == -1)
  {
    perror("ioctl (VIDIOCSPICT)");
    return;
  }
}

void setsize_cif()
{
  vid_win.x = 0;
  vid_win.y = 0;
  vid_win.width = 352;
  vid_win.height = 288;
  if (ioctl(dev, VIDIOCSWIN, &vid_win) == -1)
  {
    perror("ioctl (VIDIOCSWIN)");
    return;
  }
  return;
}

void setsize_qcif()
{
  vid_win.x = 88;
  vid_win.y = 48;
  vid_win.width = 176;
  vid_win.height = 144;
  if (ioctl(dev, VIDIOCSWIN, &vid_win) == -1)
  {
    perror("ioctl (VIDIOCSWIN)");
    return;
  }
  return;
}

int main(int argc, char *argv[])
{
  unsigned long count = 0, j = 0;
  int interval = 4, qcif = 0;
  char filename[128];
  char hostname[64];
  char device[64];
  char fprefix[64];
  char startdir[128];
  char ch;
  int verbose = 0;
  int bg = 0;
  int dolink = 0;
  int pid;

  signal(SIGINT, _sighandler);

  if ((grab_data = malloc(MAX_RGBA_IMAGE_SIZE)) == NULL)
  {
    perror("Error allocating memory for grabbing.\n");
    return 0;
  }

  strcpy(device, "/dev/video");
  strcpy(fprefix, "out");
  startdir[0] = 0;

  while ((ch = getopt(argc, argv, "qi:c:d:vsjOQ:S:f:D:bl")) != EOF)
    switch ((char)ch)
    {
    case 'S':
      jsmooth = atoi(optarg);
      break;
    case 'O':
      jopt = 1;
      break;
    case 'Q':
      jquality = atoi(optarg);
      break;
    case 'b':
      bg = 1;
      break;
    case 'q':
      qcif = 1;
      break;
    case 's':
      sequential = 1;
      break;
    case 'i':
      interval = atoi(optarg);
      break;
    case 'f':
      strcpy(fprefix, optarg);
      break;
    case 'D':
      strcpy(startdir, optarg);
      break;
    case 'd':
      strcpy(device, optarg);
      break;
    case 'c':
      count = atol(optarg);
      break;
    case 'v':
      verbose = 1;
      break;
    case 'l':
      dolink = 1;
      break;
    case 'h':
    default:
      usage();
      exit(4);
    }

  if (argc < 2 || count == 0)
  {
    usage();
    exit(5);
  }

  if (jsmooth < 0 || jsmooth > 100)
    jsmooth = 0;
  if (jquality < 0 || jquality > 100)
    jquality = 60;

  if (jopt < 0 || jopt > 100)
    jopt = 1;

  if (verbose)
    printf("smooth:%d, quality:%d, optimize:%d\n", jsmooth, jquality, jopt);

  if (gethostname(hostname, 64))
  {
    perror("gethostname");
    exit(errno);
  };

  if (startdir[0] && chdir(startdir))
  {
    perror(startdir);
    exit(errno);
  }
  dev = open(device, O_RDWR);
  if (dev < 0)
  {
    perror(device);
    exit(1);
  }

  vid_pic.depth = 0;

  ioctl(dev, VIDIOCGCAP, &vid_caps);
  ioctl(dev, VIDIOCGWIN, &vid_win);
  ioctl(dev, VIDIOCGPICT, &vid_pic);

  int ib = (vid_pic.brightness) / 256;
  int ic = (vid_pic.contrast) / 256;
  int iw = (vid_pic.colour) / 256;

  printf("Brightness: %d, Contrast: %d, Colour: %d\n", ib, ic, iw);

  /*  vid_pic.palette = VIDEO_PALETTE_RGB24;
  bytes_per_rgb = 3;
  vid_pic.depth = 24;*/

  vid_pic.palette = VIDEO_PALETTE_RGB24;
  bytes_per_rgb = 3;

  /*  if(ioctl (dev, VIDIOCSPICT, &vid_pic)) {
    perror("wrong palette/depth");
    exit( 1 );
  }
*/
  vid_win.x = 0;
  vid_win.y = 0;
  vid_win.width = 352;
  vid_win.height = 288;

  if (ioctl(dev, VIDIOCSWIN, &vid_win) == -1)
  {
    perror("ioctl (VIDIOCSWIN)");
    exit(1);
  }

  if (qcif)
    setsize_qcif();
  else
    setsize_cif();

  if (bg && (pid = fork()))
  {
    if (verbose)
      printf("forking\n");
    exit(0);
  }

  for (j = 0; j < count || count < 0; j++)
  {
    if (count < 0)
      j = 0;
    if (verbose)
    {
      printf("iteration:%ld\n", j);
      printf("count:%ld\n", count);
      printf("sequential:%d\n", sequential);
      printf("sleeping for: %d seconds\n", interval);
    }
    sleep(interval);
    if (verbose)
      printf("grabbing...");

    fflush(stdout);
    grab();
    swap_rgb24(grab_data);
    //grab_mmap();
    if (verbose)
      printf("ok\n");
    //strftime(time_buff,128,"%Y%j-%T",localtime(&thetime));
    //sprintf(filename,"%s-spy-%s",hostname,time_buff);
    sprintf(filename, "mediabox");
    if (sequential)
      sprintf(filename, "%s%02ld", fprefix, j);
    strcat(filename, ".jpg");
    if (verbose)
      printf("Writting %s\n", filename);
    errno = export_jpeg(filename);
    if (dolink)
    {
      unlink("current.jpg");
      if (symlink(filename, "current.jpg"))
      {
        perror("symlink:current.jpg");
      }
    }
  }
  free(grab_data);
  //munmap (grab_data,MAX_RGBA_IMAGE_SIZE);
  return 0;
}

void swap_rgb24(unsigned char *data)
{
  unsigned char c;
  unsigned char *p = data;
  int i = vid_win.width * vid_win.height;
  while (--i)
  {
    c = p[0];
    p[0] = p[2];
    p[2] = c;
    p += 3;
  }
}

int export_jpeg(char *filename)
{

  char *fileout = filename;
  unsigned char *img = grab_data;
  int lx = vid_win.width;
  int ly = vid_win.height;
  //int lw=vid_pic.depth;
  //int lw = 8;

  FILE *fp;
  unsigned char *line; // pointer to beggining of line
  unsigned int linesize = lx * 3, i;
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;

  if (!strncmp(fileout, "-", 1))
  {
    fp = stdout; // we dump the file to stdout
  }
  else
  {
    if ((fp = fopen(fileout, "w")) == NULL)
    {
      perror("fopen");
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
  if (vid_pic.depth == 1)
    cinfo.in_color_space = JCS_GRAYSCALE;
  else
    cinfo.in_color_space = JCS_RGB;

  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, jquality, TRUE);
  jpeg_start_compress(&cinfo, TRUE);

  line = img;

  for (i = 1; i <= ly; i++)
  {
    jpeg_write_scanlines(&cinfo, &line, 1);
    line = img + (linesize * i);
  }

  jpeg_finish_compress(&(cinfo));
  jpeg_destroy_compress(&(cinfo));
  fclose(fp);
  return (0);
}
