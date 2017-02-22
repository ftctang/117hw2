/**
 *  \file mandelbrot_serial.cc
 *  \brief Lab 2: Mandelbrot set master slave code
 Refrence: https://service.clustervision.com/content/example-mpi-communication-skeleton
 */


#include <iostream>
#include <cstdlib>
#include <mpi.h>
#include "render.hh"

using namespace std;

#define WIDTH 1000
#define HEIGHT 1000

int ntasks;

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

int master(double minX , double minY , double maxX , double maxY , double it , double jt , int height , int width ){
	printf("Job being set to process\n");
	
	// Setting up arrays 
	int rbuffer[width+1];//Width + row index 
	int image[height*width];
  
	int rank;
	int RowNum = 0 ; 
	int RowIndex; 
	MPI_Comm_size(MPI_COMM_WORLD , &ntasks);
	MPI_Status status ;
	double t_start , t_elapsed ; 
    t_start = MPI_Wtime (); /* Start timer */
	
	/* Seed Slaves*/
	for(rank = 1 ; rank < ntasks; ++rank){
		MPI_Send(&RowNum, 1 , MPI_INT , rank , 0 , MPI_COMM_WORLD ) ;
		if(rank >= height){ //Initial processors exceed height end
			MPI_Send(&RowNum, 1 , MPI_INT , rank , 2 , MPI_COMM_WORLD ) ;
		}
		RowNum++;
	}
	
	/* Receive a result from slave and dispatch a new work request */ 
	/* Tag0 is for Rows. Tag1 is for data. Tag2 is for  out of rows . Tag3 is kill tag. */
	while (RowNum < height) { 
		MPI_Recv(rbuffer , width + 1  , MPI_INT , MPI_ANY_SOURCE  , MPI_ANY_TAG , MPI_COMM_WORLD , &status) ;  
		MPI_Send(&RowNum, 1 , MPI_INT , status.MPI_SOURCE , 0 , MPI_COMM_WORLD );
		RowIndex = rbuffer[width];
		memcpy(image + (RowIndex *width) , rbuffer , width*sizeof(int));	
		RowNum++;
	}	
	
	/* Receive results for work requests*/
	for(rank = 1 ; rank < ntasks ; rank++){
		MPI_Recv(rbuffer , width + 1  , MPI_INT, MPI_ANY_SOURCE  , MPI_ANY_TAG , MPI_COMM_WORLD , &status) ;
		if(status.MPI_TAG == 1){
			RowIndex = rbuffer[width];
			memcpy(image + (RowIndex *width) , rbuffer , width*sizeof(int));		
		}
	}
	
	//Tell all slaves to Exit
	for(rank = 1 ; rank < ntasks ; rank++){ // Tag 3 is for kill tag. 
		MPI_Send(&RowNum, 1 , MPI_INT, rank , 3 , MPI_COMM_WORLD );
	}
	
	//MPI_Barrier(MPI_COMM_WORLD);
	gil::rgb8_image_t img(height, width);
    auto img_view = gil::view(img);	
	  /* Writes back all values */
	for (int i = 0; i < height; ++i) {
		for (int j = 0; j < width; ++j) {
			img_view(j,i) = render(image[(i*width) + j]/512.0);
		}
	}	    
	gil::png_write_view("mandelbrotms.png", const_view(img));
	t_elapsed = MPI_Wtime () - t_start; /* Stop timer */
	printf("Timestamp: %f\n", t_elapsed);
	return 0;
}

int slave(double minX , double minY , double maxX , double maxY , double it , double jt , int height , int width ){ //Slave process that calculates mandelbrot
	MPI_Status status ;
	int RowNum;
	double x , y ; 
	int sbuffer[width +1 ] ; //Width of the Row + the ID of the row in the last index  
	while(true){
		MPI_Recv(&RowNum , 1  , MPI_INT , 0  , MPI_ANY_TAG , MPI_COMM_WORLD , &status) ;  
		if(status.MPI_TAG != 3 && status.MPI_TAG != 2 ) { // Tag to end slave process 
			//To know what y to start at 
			y = minY + (RowNum * it) ;
			
		   //Mandlebrot 
			x = minX;
			for (int j = 0; j < width; ++j) {
				sbuffer[j] = mandelbrot(x, y);
				x += jt ;
			}
			y += it;
			sbuffer[width]= RowNum;
			MPI_Send(&sbuffer, width+1 , MPI_INT, 0 , 1, MPI_COMM_WORLD );
		}
		
		else if(status.MPI_TAG == 2 ){//Stall process 
			MPI_Send(&sbuffer, width+1 , MPI_INT, 0 , 2, MPI_COMM_WORLD );
		}
		
		else{//KIll PROCESS
			return 0;
		}
		
	}	
}


int
main(int argc, char* argv[]) {
	
  int np = 0 ;
  int rank = 0 ;
  int namelen = 0;
  char hostname[MPI_MAX_PROCESSOR_NAME+1];
  
  MPI_Init (&argc, &argv);	/* starts MPI */
  MPI_Comm_rank (MPI_COMM_WORLD, &rank);	/* Get process id */
  MPI_Comm_size (MPI_COMM_WORLD, &np);	/* Get number of processes */ 
  MPI_Get_processor_name (hostname, &namelen); /* Get hostname of node */
  
  double minX = -2.1;
  double maxX = 0.7;
  double minY = -1.25;
  double maxY = 1.25;
  
  /* Block Variables*/
  int height, width;  
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
	
  if(rank ==  0){ //Master
	master(minX , minY , maxX , minY , it , jt , height , width);
  }
  
  else{
	slave(minX , minY , maxX , minY , it , jt , height , width);  
  }
  
  
  MPI_Finalize();

  return 0;

}


/* eof */
