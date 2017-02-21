/**
 *  \file mandelbrot_susie.cc
 *
 *  \brief Implement your parallel mandelbrot set in this file.
 */

#include <iostream>
#include <cstdlib>
#include <mpi.h>
#include "render.hh"

using namespace std;

#define WIDTH 1000
#define HEIGHT 1000

int
mandelbrot(double x, double y) {
  int maxit = 511;
  double cx = x;
  double cy = y;
  double newx, newy;

  int it = 0;
  for (it = 0; it < maxit && (x*x + y*y) < 4; ++it) {
    newx = x*x - y*y + cx;
    newy = 2*x*y + cy;
    x = newx;
    y = newy;
  }
  return it;
}

int
main (int argc, char* argv[])
{
  /* Lucky you, you get to write MPI code */
  double minX = -2.1;
  double maxX = 0.7;
  double minY = -1.25;
  double maxY = 1.25;
  double t_start, t_elapsed;
  
  int height, width;
  
  int rank = 0, np = 0, namelen = 0;
  char hostname[MPI_MAX_PROCESSOR_NAME+1];
  
  MPI_Init(&argc, &argv); //starts MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank); //get process id
  MPI_Comm_size(MPI_COMM_WORLD, &np); //get number of processes
  MPI_Get_processor_name(hostname, &namelen); //get hostname of nodes
  
  //timer start
  if(rank == 0){
	  t_start = MPI_Wtime();
  }
  
  if (argc == 3) {
    height = atoi (argv[1]);
    width = atoi (argv[2]);
    assert (height > 0 && width > 0);
  } else {
    fprintf (stderr, "usage: %s <height> <width>\n", argv[0]);
    fprintf (stderr, "where <height> and <width> are the dimensions of the image.\n");
    return -1;
  }

  double it = (maxY - minY)/height;
  double jt = (maxX - minX)/width;
  double x, y;


  gil::rgb8_image_t img(height, width);
  auto img_view = gil::view(img);
  
  //array setup
  float *rbuffer = new float[width*height];
  float *temp = new float[height/np*width];
  float **image = new float *[height];

  for(int j = 0; j < height; j++){
          image[j] = new float[width];
  }
  
  int n = 0;
  
  //mandelbrot
  y = minY;
  for (int i = rank + np * n; i < height; ++i) {
	i = rank + np * n;
	if(i < height){
		x = minX;
		for (int j = 0; j < width; ++j) {
			temp[i * width + j] = mandelbrot(x, y)/512.0;
			x += jt;
    }
		y += it;
		n++;
	}
    
  }
  
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Gather(temp, height/np * width, MPI_FLOAT, rbuffer, height/np * width, MPI_FLOAT, 0, MPI_COMM_WORLD);
  
  
  //still needs to be fixed
  if(rank == 0){
	  for(int i = 0; i < height; ++i){
		  for(int j = 0; j < width; ++j){
			  image[i][j] = rbuffer[i * width + j];
			  img_view(j, i) = render(image[i][j]);
		  }
	  }
	  gil::png_write_view("mandelbrotsusie.png", const_view(img));
  }
  
  
  MPI_Finalize();
  if(rank == 0){
	  t_elapsed = MPI_Wtime() - t_start;
	  printf("Timestamp: %f\n", t_elapsed);
  }
}

/* eof */
