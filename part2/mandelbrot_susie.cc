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
  
  //retrieve dimensions of the image from user input
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
  
  //array setup
  int rows = (height / np) + 1; 	
  int length = rows * width;		
  int *rbuffer = new int[length];	//size of buffer
  int n = 0;
  
  //mandelbrot
  y = minY + (rank * it);
  int row = 0;
  for (int i = rank; i < height; i += np) {
	x = minX;
	for (int j = 0; j < width; ++j) {
		rbuffer[(row * width) + j] = mandelbrot(x, y);
		x += jt;
	}
	y += (it * np);
    row++;
  }
  
  MPI_Barrier(MPI_COMM_WORLD);
  
  //receiving data
  int* R = NULL;
  if(rank == 0){
	  R = new int[length * np];
  }
  
  //gather all data from each processor
  MPI_Gather(rbuffer, length, MPI_INT, R, length, MPI_INT, 0, MPI_COMM_WORLD);
  
  //output the image
  if(rank == 0){
	    gil::rgb8_image_t img(height, width);
		auto img_view = gil::view(img);
		int o = 0;
		int p = 0;
	  
	  for(int i = 0; i < height; ++i){
		  for(int j = 0; j < width; ++j){
			  o = (i % np) * length;
			  img_view(j, i) = render(R[o + (p * width) + j] / 512.0);
		  }
		  p = i / np;
	  }
	  t_elapsed = MPI_Wtime() - t_start;
	  printf("Timestamp: %f\n", t_elapsed);
	  gil::png_write_view("mandelbrotsusie.png", const_view(img));
  }
  
  
  MPI_Finalize();
}

/* eof */
