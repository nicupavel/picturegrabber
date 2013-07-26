# PictureGrabber

Picture Grabber is a video 4 linux capture application able to read raw data from a V4L compatible device and output to a jpeg file. 
It works on both V4L1 and V4L2 APIs and it used libjpeg to create the jpeg from the data captured. PictureGrabber aims for a very small footprint (currently binary is about 4k) and only implements the basic features 
needed to capture data and transform it to a jpeg.

Picture Grabber can be used with any V4L compatible device (capture cards, webcam, usb v4l devices).

	Usage: picturegrabber [OPTION] 
		-c <num> number of shots,-1=unlimited (option MANDATORY) 
		-i <sec> interval between shots in seconds (default=4) 
		-d <dev> device to open (default=/dev/video) 
		-D <dir> output in this directory 
		-q qcif 176x144 output (defaults to cif 352x288)
		-s use sequential filename numbering (out1,out2...) -f <str> filename prefix (used with -s, default=out) 
		-v be verbose 
		-b run in background as a daemon 
		-O coding optimization (default=no) 
		-Q <num> quality factor [0-100] (default=60) 
		-S <num> smoothing factor [0-100] (default=0) 
		-l link last saved file as current.jpg

