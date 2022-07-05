#include <stdio.h>
#include <stdbool.h>
#include <mpi.h>
#include <pthread.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "assignment2.h"

#define NUMTHREAD 3
#define TIMEOUT 10
#define THRESHOLD_RANGE 100
#define MAX_ELEMENT 50
#define MAX_ITERATION 5

#define SHIFT_ROW 0 //to know your up or down neighbour
#define SHIFT_COL 1 //to know your left or right neighbour 
#define DISP 1 //immediate neighbours, if 2 means 2 blocks from current node

#define REPORT_TAG 2
#define EXIT_TAG 3


//An array of reportstruct for the satelitle altimeter to store the reporting information for every repeating cycle
struct reportstruct rptsSA[MAX_ELEMENT];
//Global variables
int alertTrue = 0;
int alertFalse = 0;
int logCount = 0;
int comm_time = 0;
int nslaves;
int exit_thread = 0;
int count = 0;
FILE *fp;
pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
double totalCommTime;

int main(int argc, char **argv) {
    int rank, size;
    MPI_Comm new_comm;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    //Split the comm world into the master and slave program
    MPI_Comm_split( MPI_COMM_WORLD,rank == size-1, 0, &new_comm);
    if (rank == size-1) 
        master_io( MPI_COMM_WORLD, new_comm );
    else
        slave_io( MPI_COMM_WORLD, new_comm, argc, argv);
    
    MPI_Finalize();
    return 0;

}

// Main function carried out by each thread for the base station 
void *performFunction(void *arg) {
    int pid = *(int*) arg;
    int messages = 0;
	int alerts = 0;
	int i;
	int flag;
    
    struct reportstruct rpts;
	
	MPI_Datatype Valuetype;
	MPI_Datatype type[10] = {MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_FLOAT, MPI_DOUBLE};
	int blocklen[10] = {1,2,2,2,2,2,6,1,1,1};
	MPI_Aint disp[10];
	
	MPI_Get_address(&rpts.rank, &disp[0]);
	MPI_Get_address(&rpts.coor, &disp[1]);
	MPI_Get_address(&rpts.coor_up, &disp[2]);
	MPI_Get_address(&rpts.coor_down, &disp[3]);
	MPI_Get_address(&rpts.coor_left, &disp[4]);
	MPI_Get_address(&rpts.coor_right, &disp[5]);
	MPI_Get_address(&rpts.datetime, &disp[6]);
	MPI_Get_address(&rpts.message_compared, &disp[7]);
	MPI_Get_address(&rpts.SW_height, &disp[8]);
	MPI_Get_address(&rpts.comm_time, &disp[9]);
	
	//Make relative
	disp[9]=disp[9]-disp[8];
	disp[8]=disp[8]-disp[7];
	disp[7]=disp[7]-disp[6];
	disp[6]=disp[6]-disp[5];
	disp[5]=disp[5]-disp[4];
	disp[4]=disp[4]-disp[3];
	disp[3]=disp[3]-disp[2];
	disp[2]=disp[2]-disp[1];
	disp[1]=disp[1]-disp[0];
	disp[0] = 0;
	
	//Create MPI struct
	MPI_Type_create_struct(10, blocklen, disp, type, &Valuetype);
	MPI_Type_commit(&Valuetype);
    
    if(pid == 0) {
    //This thread is for base station to produce log summary for each iteration
		for (i = 0; i < MAX_ITERATION; i++) {
				
			if (i == MAX_ITERATION - 1) {

				shutDownSensorNode();
				// MPI send to terminate SN to break the loop
				// send to terminate altimeter
				exit_thread = 1;
				logSummary();
			}
			
			sleep(5);      // need to be longer than sensor node and altimeter
			// another theory is that set a timeout of 10s for iprobe, then go for next iteration
		}
		
		fclose(fp);
    } else if (pid == 1) {
    //This thread is for the satelite altimeter to constantly produce sea water reading 
        srand(time(0));

        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        

        while (true)
        {
            struct reportstruct report;
            report.datetime[0] =  tm.tm_year + 1900;
            report.datetime[1] = tm.tm_mon + 1;
            report.datetime[2] = tm.tm_mday;
            report.datetime[3] = tm.tm_hour;
            report.datetime[4] = tm.tm_min;
            report.datetime[5] = tm.tm_sec;
            report.coor[0] = rand() % 3 + 0;
            report.coor[1] = rand() % 3 + 0;
            report.SW_height = (rand() % (7000 - 6000 + 1)) + 6000;
            
            if (count == MAX_ELEMENT) {
                pop();
                count--;
            }
            
            push(count,report);
            
            count++;
            
            //If exit command is true then altimeter will stop producing sea water value
            if(exit_thread) {
            	printf("break: %d\n", exit_thread);
            	break;
            }
       }
    } else {
    //This thread is for base station to compare the report data sent from the sensor node and altimeter
    
        struct resultstruct altimeterReading;
        MPI_Status status, probe_status;
        MPI_Request request;
        	
		for (i = 0; i < MAX_ITERATION; i++) {

			MPI_Iprobe(MPI_ANY_SOURCE, REPORT_TAG, MPI_COMM_WORLD, &flag, &probe_status);
			//status
			printf("flag: %d %d\n", flag, status.MPI_SOURCE);
			
			if (flag) {
			    flag = 0;
				MPI_Recv(&rpts, 20, Valuetype, status.MPI_SOURCE, REPORT_TAG, MPI_COMM_WORLD, &status);
				
				//start
				altimeterReading = verify(rpts, rptsSA);
				
				//end
				logFile(i, rpts, altimeterReading);
				
				printf("Writing data entry into the log file for iteration %d\n", i);
				
				/*
				if (rpts.message_compared) {
				    
				} else {
				    // report fault detection
				} */
			}
			sleep(4);
		}
			
		printf("Finsih writing entry to the log file\n");
		
		}
}

//Pop function to pop old altimeter reading from an array to make space for new reading
void pop() {
    int i;
    for (i=0; i < MAX_ELEMENT - 1; i++) {
        rptsSA[i] = rptsSA[i+1];
    }
}

//Push function to push new altimeter reading to an array
void push(int counter, struct reportstruct report) {
        rptsSA[counter] = report;
}

//Function to compare matching coordinates and threshold range between sensor nodes and altimeter
struct resultstruct verify(struct reportstruct sensorNode, struct reportstruct *sharedArray) {
    
	int i;
	struct resultstruct result;
	
	for (i = 0; i < count; i++) {
		
	    if (sharedArray[i].coor[0] == sensorNode.coor[0] && sharedArray[i].coor[1] == sensorNode.coor[1] && checkTime(sharedArray[i], sensorNode) == 1) {
	    
	        result.report.coor[0] = sensorNode.coor[0];
	        result.report.coor[1] = sensorNode.coor[1];
	        result.report.SW_height = sharedArray[i].SW_height;
	        result.report.datetime[0] = sharedArray[i].datetime[0];
	        result.report.datetime[1] = sharedArray[i].datetime[1];
	        result.report.datetime[2] = sharedArray[i].datetime[2];
	        result.report.datetime[3] = sharedArray[i].datetime[3];
	        result.report.datetime[4] = sharedArray[i].datetime[4];
	        result.report.datetime[5] = sharedArray[i].datetime[5];
	        if (abs(sensorNode.SW_height - sharedArray[i].SW_height) <= THRESHOLD_RANGE) {
	            result.alert = 1;
	        } else {
	            result.alert = 0;
	        }
	    }
	}
	return result;
}

//Function to check the timestamp for sea water reading between the sensor nodes and altimeter
int checkTime(struct reportstruct sharedArray, struct reportstruct sensorNode) {

	int h, m, s;
	
	h = abs(sharedArray.datetime[3] - sensorNode.datetime[3]);
	
	m = abs(sharedArray.datetime[4] - sensorNode.datetime[4]);
	
	s = abs(sharedArray.datetime[5] - sensorNode.datetime[5]);
	
	int time_diff = (h*3600) + (m*60) + s; //get time difference in seconds
	
	if (time_diff <= TIMEOUT)
		return 1;
    else
    	return 0;
}

//Function to send exit command to the sensor nodes
void shutDownSensorNode() {
	int value = 1;
    for (int i = 0; i < nslaves ; i++) {
        MPI_Send(&value, 1, MPI_INT, i, EXIT_TAG, MPI_COMM_WORLD);
        printf("shutdown: %d\n", i);
    }
}

//The master program which is being executed by the last node
int master_io(MPI_Comm world_comm, MPI_Comm comm)
{
    int size, i;
    MPI_Status status;
    MPI_Comm_size(world_comm, &size );
    nslaves = size - 1;

    pthread_t hThread[NUMTHREAD];
    int threadNum[NUMTHREAD];
    
    //write the output elements of each process to a file
    fp = fopen("report.txt", "w");
    
    for(i=0; i < NUMTHREAD; i++){
		threadNum[i] = i;
		pthread_create(&hThread[i], NULL, performFunction, &threadNum[i]);
	}
	
	for(i=0; i < NUMTHREAD; i++){
		pthread_join(hThread[i], NULL);
	} 

    return 0;
}

//The slave program which is being executed by all nodes except the last node
int slave_io(MPI_Comm world_comm, MPI_Comm comm,int argc, char **argv) {
    int ndims=2, size, my_rank, reorder, my_cart_rank, ierr, world_size;
	int nrows, ncols;
	int down_neigh, up_neigh; //up and down neighbour
	int left_neigh, right_neigh; //left and right neighbour
	MPI_Comm comm2D;
	int dims[ndims],coord[ndims];
	int wrap_around[ndims];
	int flag;
	
	/* start up initial MPI environment */
	MPI_Comm_size(world_comm, &world_size);
	MPI_Comm_size(comm, &size);
	MPI_Comm_rank(comm, &my_rank);
	
	/*Creating a new datatype for type reportstruct */
	struct reportstruct values;
	MPI_Datatype Valuetype;
	MPI_Datatype type[10] = {MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_FLOAT, MPI_DOUBLE};
	int blocklen[10] = {1,2,2,2,2,2,6,1,1,1};
	MPI_Aint disp[10];
	
	MPI_Get_address(&values.rank, &disp[0]);
	MPI_Get_address(&values.coor, &disp[1]);
	MPI_Get_address(&values.coor_up, &disp[2]);
	MPI_Get_address(&values.coor_down, &disp[3]);
	MPI_Get_address(&values.coor_left, &disp[4]);
	MPI_Get_address(&values.coor_right, &disp[5]);
	MPI_Get_address(&values.datetime, &disp[6]);
	MPI_Get_address(&values.message_compared, &disp[7]);
	MPI_Get_address(&values.SW_height, &disp[8]);
	MPI_Get_address(&values.comm_time, &disp[9]);
	
	//Make relative
	disp[9]=disp[9]-disp[8];
	disp[8]=disp[8]-disp[7];
	disp[7]=disp[7]-disp[6];
	disp[6]=disp[6]-disp[5];
	disp[5]=disp[5]-disp[4];
	disp[4]=disp[4]-disp[3];
	disp[3]=disp[3]-disp[2];
	disp[2]=disp[2]-disp[1];
	disp[1]=disp[1]-disp[0];
	disp[0] = 0;
	
	//Create MPI struct
	MPI_Type_create_struct(10, blocklen, disp, type, &Valuetype);
	MPI_Type_commit(&Valuetype);
	
	
	/* process command line arguments*/
	//Specified row and column
	if (argc == 3) {
		nrows = atoi (argv[1]);
		ncols = atoi (argv[2]);
		dims[0] = nrows; /* number of rows */
		dims[1] = ncols; /* number of columns */
		if( (nrows*ncols) != size) {
			if( my_rank ==0) printf("ERROR: nrows*ncols)=%d * %d = %d != %d\n", nrows, ncols, nrows*ncols,size);
			MPI_Finalize(); 
			return 0;
		}
	} else {
		//Not specified row and column
		nrows=ncols=(int)sqrt(size);
		dims[0]=dims[1]=0; // if zero by default it will create the best grid size for u
	}
	
	/*************************************************************/
	/* create cartesian topology for processes */
	/*************************************************************/
	MPI_Dims_create(size, ndims, dims);
	
	if(my_rank==0)
		printf("Root Rank: %d. Comm Size: %d: Grid Dimension = [%d x %d] \n",my_rank,size,dims[0],dims[1]);
	
	/* create cartesian mapping */
	wrap_around[0] = 0;
	wrap_around[1] = 0; /* periodic shift is .false. */
	reorder = 1;
	ierr =0;
	ierr = MPI_Cart_create(comm, ndims, dims, wrap_around, reorder, &comm2D);
	if(ierr != 0) printf("ERROR[%d] creating CART\n",ierr);
	
	/* find my coordinates in the cartesian communicator group */
	MPI_Cart_coords(comm2D, my_rank, ndims, coord); // coordinated is returned into the coord array
	/* use my cartesian coordinates to find my rank in cartesian group*/
	MPI_Cart_rank(comm2D, coord, &my_cart_rank);
	/* get my neighbors; axis is coordinate dimension of shift */
	/* axis=0 ==> shift along the rows: P[my_row-1]: P[me] : P[my_row+1] */
	/* axis=1 ==> shift along the columns P[my_col-1]: P[me] : P[my_col+1] */
	
	MPI_Cart_shift( comm2D, SHIFT_ROW, DISP, &down_neigh, &up_neigh ); // to know top and bottom nodes the second argument cartshift should be 0
	MPI_Cart_shift( comm2D, SHIFT_COL, DISP, &left_neigh, &right_neigh ); // to know left and right nodes the second argument cartshift should be 1
	
	//printf("Global rank: %d. Cart rank: %d. Coord: (%d, %d). Left: %d. Right: %d. Top: %d. Bottom: %d\n", my_rank, my_cart_rank, coord[0], coord[1], left_neigh, right_neigh, down_neigh, up_neigh);
	//fflush(stdout);

	do{
	
		float lower = 5900.000; 
		float upper = 6100.000;
		double start, end, time_taken;
	
		int read = 2; // number of readings per node to calculate average
		float SWH_arr[read]; // sea water height array
	
		// Iterate through 5 times to fill up the SWH_arr first
    		for (int j = 0; j < read; j ++) {
	
			sleep(my_rank);
			unsigned int seed = time(NULL);
			float randomVal = rand_r(&seed)/ (float) RAND_MAX; // [0,1.0] 
			randomVal = lower + randomVal * (upper -lower); // [lower,upper]
			
			SWH_arr[j] = randomVal; // store current float value to the SWH array
		
		}
		
		//Calculate the simple moving average
		float SMP = 0;
		for (int i =0; i<read; i++){
		    SMP += SWH_arr[i];
		}
		SMP = SMP/read;
		
		MPI_Request send_request[4];
		MPI_Request receive_request[4];
		MPI_Status send_status[4];
		MPI_Status receive_status[4];
		
		start = MPI_Wtime();
		
		//Send SMP value to adjacent nodes
		MPI_Isend(&SMP, 1, MPI_FLOAT, down_neigh, 0, comm2D, &send_request[0]);
		MPI_Isend(&SMP, 1, MPI_FLOAT, up_neigh, 0, comm2D, &send_request[1]);
		MPI_Isend(&SMP, 1, MPI_FLOAT, left_neigh, 0, comm2D, &send_request[2]);
		MPI_Isend(&SMP, 1, MPI_FLOAT, right_neigh, 0, comm2D, &send_request[3]);
				
		float recvValL = -1.0, recvValR = -1.0, recvValT = -1.0, recvValB = -1.0;
		//Receive SMP values from adjacent nodes
		MPI_Irecv(&recvValT, 1, MPI_INT, down_neigh, 0, comm2D, &receive_request[0]);
		MPI_Irecv(&recvValB, 1, MPI_FLOAT, up_neigh, 0, comm2D, &receive_request[1]);
		MPI_Irecv(&recvValL, 1, MPI_FLOAT, left_neigh, 0, comm2D, &receive_request[2]);
		MPI_Irecv(&recvValR, 1, MPI_FLOAT, right_neigh, 0, comm2D, &receive_request[3]);
		
		MPI_Waitall(4, send_request, send_status);
		MPI_Waitall(4, receive_request, receive_status);
		
		end = MPI_Wtime();
		time_taken = end - start; 
		
		//printf("Global rank: %d. Cart rank: %d. Coord: (%d, %d). SMP: %f. Top: %f. Bottom: %f. Left: %f. Right: %f.\n", my_rank, my_cart_rank, coord[0], coord[1], SMP, recvValT, recvValB, recvValL, recvValR);
		
		//Compare moving average with adjacent nodes if threshold > 6000 and check if the tolerance range between adjacent nodes is <= 50 meters then increase threshold_count for each valid comparison
		int threshold_count = 0;
		time_t t = time(NULL);
	  	struct tm tm = *localtime(&t);
		if (SMP > 6000){

			if (abs(SMP - recvValT) <= THRESHOLD_RANGE )
				threshold_count ++;
			if (abs(SMP - recvValB) <= THRESHOLD_RANGE)
				threshold_count ++;
			if (abs(SMP - recvValL) <= THRESHOLD_RANGE )
				threshold_count ++;
			if (abs(SMP - recvValR) <= THRESHOLD_RANGE )
				threshold_count ++;
		}
		
		
		values.rank = my_rank;
		values.coor[0] = coord[0], values.coor[1] = coord[1];
		values.datetime[0] = tm.tm_year +1900, values.datetime[1] = tm.tm_mon + 1;
		values.datetime[2] = tm.tm_mday, values.datetime[3] = tm.tm_hour;
		values.datetime[4] = tm.tm_min, values.datetime[5] = tm.tm_sec;
		values.message_compared = threshold_count;
		values.SW_height = SMP;
		values.comm_time = time_taken;
		
		//Check if the adjacent nodes are part of cartesian grid then indicate 1 as true and 0 as false
		values.coor_down[0] = down_neigh;
		values.coor_up[0] = up_neigh;
		values.coor_left[0] = left_neigh;
		values.coor_right[0] = right_neigh;
		
		values.coor_down[1] = recvValT;
		values.coor_up[1] = recvValB;
		values.coor_left[1] = recvValL;
		values.coor_right[1] = recvValR;
		
	
		//Matched SMP reading from adjacent nodes is >= 2 then send a report to the base station
		if (threshold_count >= 2){
			
			printf("Rank: %d. down_neigh: %d. up_neigh: %d. left_neigh: %d. right_neigh: %d\n", my_rank, values.coor_down[0], values.coor_up[0], values.coor_left[0], values.coor_right[0]);
			
			
			//Send the reportstruct values to master comm
			MPI_Send( &values, 20, Valuetype, world_size-1, REPORT_TAG, world_comm);	
		}
		
		//Constantly read for incoming message sent using the EXIT_TAG indentifier
		MPI_Iprobe(MPI_ANY_SOURCE, EXIT_TAG, MPI_COMM_WORLD, &flag, MPI_STATUS_IGNORE);
		if (flag) {
			int value;
			//Receive the exit command sent by the base station 
			MPI_Recv(&value, 1, MPI_INT, world_size-1, EXIT_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			break;	
		}
		
		
		
	}while(true);
	
	printf("Rank_exit: %d\n", my_rank);
	
	MPI_Comm_free( &comm2D );
	return 0;
    
}

//Function for base station to write data to the report.txt file
void logFile(int iteration, struct reportstruct sensorNode, struct resultstruct altimeter) {
	
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    fprintf(fp, "------------------------------------------------\n");
    fprintf(fp, "Iteration: %d\n", iteration);
    fprintf(fp, "Logged time: %d-%02d-%02d %02d:%02d:%02d\n", tm.tm_year +1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);             
    fprintf(fp, "Alert reported time: %d-%02d-%02d %02d:%02d:%02d\n", sensorNode.datetime[0], sensorNode.datetime[1], sensorNode.datetime[2], sensorNode.datetime[3], sensorNode.datetime[4], sensorNode.datetime[5]);
    if (altimeter.alert == 1) {
        fprintf(fp, "Alert type: True Alert\n\n");
        alertTrue++;
    } else if (altimeter.alert == 0) {
        fprintf(fp, "Alert type: False Alert\n\n");
        alertFalse++;
    }
    
    fprintf(fp, "Reporting Node\tCoordinates\tHeight(m)\t\n");
    fprintf(fp, "%d\t(%d, %d)\t%f\n\n", sensorNode.rank, sensorNode.coor[0], sensorNode.coor[1], sensorNode.SW_height);
    fprintf(fp, "Adjacent Nodes\tCoordinates\tHeight(m)\t\n");
    if (sensorNode.coor_down[0] >= 0) 
        fprintf(fp, "%d\t(%d, %d)\t%d\n\n", sensorNode.coor_down[0], sensorNode.coor[0], sensorNode.coor[1] - 1, sensorNode.coor_down[1]); 
    if (sensorNode.coor_up[0] >= 0) 
        fprintf(fp, "%d\t(%d, %d)\t%d\n\n", sensorNode.coor_up[0], sensorNode.coor[0], sensorNode.coor[1] + 1, sensorNode.coor_up[1]); 
    if (sensorNode.coor_left[0] >= 0) 
        fprintf(fp, "%d\t(%d, %d)\t%d\n\n", sensorNode.coor_left[0], sensorNode.coor[0] - 1, sensorNode.coor[1], sensorNode.coor_left[1]); 
    if (sensorNode.coor_right[0] >= 0) 
        fprintf(fp, "%d\t(%d, %d)\t%d\n\n", sensorNode.coor_right[0], sensorNode.coor[0] + 1, sensorNode.coor[1], sensorNode.coor_right[1]); 
    fprintf(fp, "\nSatelite altimeter reporting time: %d-%02d-%02d %02d:%02d:%02d\n", altimeter.report.datetime[0], altimeter.report.datetime[1], altimeter.report.datetime[2], altimeter.report.datetime[3], altimeter.report.datetime[4], altimeter.report.datetime[5]); 
    fprintf(fp, "Satelite altimeter reporting height (m): %f\n", altimeter.report.SW_height);
    fprintf(fp, "Satelite altimeter reporting Coordinates: (%d, %d)\n\n", altimeter.report.coor[0], altimeter.report.coor[1]);
    fprintf(fp, "Communication Time (seconds): %lf\n", sensorNode.comm_time);    // get the time
    fprintf(fp, "Total Messages send between reporting node and base station: %d\n", 1); 
    fprintf(fp, "Max. tolerance range between nodes readings (m): %d\n", THRESHOLD_RANGE); 
    fprintf(fp, "Max. tolerance range between satellite altimeter and reporting node readings (m): %d\n", THRESHOLD_RANGE);
    fprintf(fp, "------------------------------------------------\n");
    logCount++;
    totalCommTime += sensorNode.comm_time;
}

//Function to write the log summary to the report.txt file and close the file
void logSummary() {
    fprintf(fp, "Number of messages passed throughout the network when an alert is detected: %d\n", logCount);
    fprintf(fp, "Number of alerts\n\tTrue: %d\n\tFalse: %d\n", alertTrue, alertFalse);
    fprintf(fp, "Total communication time: %f\n", totalCommTime);
    
    fclose(fp);
    
}
