// A struct which stores informations needed by base station
struct reportstruct {
	int rank, coor[2], coor_up[2], coor_down[2], coor_left[2], coor_right[2];
	int datetime[6], message_compared;
	float SW_height;
	double comm_time;
};

// A struct containing datas for base station to write to the report.txt file
struct resultstruct {
    struct reportstruct report;
    int alert;
};

// Function prototype
void *performFunction(void *arg);
struct resultstruct verify(struct reportstruct sensorNode, struct reportstruct *sharedArray);
int master_io(MPI_Comm world_comm, MPI_Comm comm);
int slave_io(MPI_Comm world_comm, MPI_Comm comm,int argc, char **argv);
void logFile(int iteration, struct reportstruct sensorNode, struct resultstruct altimeter);
void shutDownSensorNode();
int checkTime(struct reportstruct sharedArray, struct reportstruct sensorNode);
void pop();
void push(int counter, struct reportstruct report);
void logSummary();
