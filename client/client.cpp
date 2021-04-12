#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <vector>


#define BUFFER_SIZE 1024
#define SERVER_PORT 8888
#define SERVER_IP "127.0.0.1"

using namespace std;

int create_and_open_fifo(const char * pname, int mode) 
{
    // creating a fifo special file in the current working directory
    // with read-write permissions for communication with the plotter
    // both proecsses must open the fifo before they can perform
    // read and write operations on it
    if (mkfifo(pname, 0666) == -1) 
    {
        cout << "Unable to make a fifo. Ensure that this pipe does not exist already!" << endl;
        exit(-1);
    }

    // opening the fifo for read-only or write-only access
    // a file descriptor that refers to the open file description is
    // returned
    int fd = open(pname, mode);

    if (fd == -1) 
    {
        cout << "Error: failed on opening named pipe." << endl;
        exit(-1);
    }

    return fd;
}

int main(int argc, char const *argv[]) 
{
/*
  Description: Reads from inpipe and writes to outpipe

  Arguments:
    int argc : Number of arguments
    char const *argv[] : Arguments stored in const array

  Returns:
    None
*/
    const char *inpipe = "inpipe";
    const char *outpipe = "outpipe";

    int in = create_and_open_fifo(inpipe, O_RDONLY);
    cout << "inpipe opened..." << endl;
    int out = create_and_open_fifo(outpipe, O_WRONLY);
    cout << "outpipe opened..." << endl;

    char ack[] = "RECEIVED";
    int S_PORT;
    string S_IP;

	if (argc == 3)
	{ // Checks if there are only 2 command line arguments
		// and stores the port number and ip address from command line
        S_PORT = stoi(argv[1]);
        S_IP=argv[2];
    }
    else
    {
        cout << "Enter port number and server IP address: \n";
        return 0;
    }
    
    // 1. Establish a connection with the server
    // sockaddr_in is the address sturcture used for Internet address 
    struct sockaddr_in my_addr, peer_addr;

    // zero out the structor variable because it has an unused part
    memset(&my_addr, '\0', sizeof my_addr);

    // Declare socket descriptor
    int socket_desc;

    char outbound[BUFFER_SIZE] = {0};
    char inbound[BUFFER_SIZE] = {0};
	char from_plot[BUFFER_SIZE] = {0};
    char outbound2[BUFFER_SIZE] = {0};

    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) 
    {
        cerr << "Listening socket creation failed!\n";
        return 1;
    }

    // Prepare sockaddr_in structure variable
    peer_addr.sin_family = AF_INET;                         
    peer_addr.sin_port = htons(S_PORT);                
    peer_addr.sin_addr.s_addr = inet_addr(S_IP.c_str());    
                                                                                           
    // connecting to the server socket
    if (connect(socket_desc, (struct sockaddr *) &peer_addr, sizeof peer_addr) == -1) 
    {
        cerr << "Cannot connect to the host!\n";
        close(socket_desc);
        return 1;
    }
    cout << "Connection established with " << inet_ntoa(peer_addr.sin_addr) << ":" << ntohs(peer_addr.sin_port) << "\n";
	
    struct timeval timeout;
    timeout.tv_sec  = 1;
    // setting timeout to 1s

    if (setsockopt(socket_desc, SOL_SOCKET, SO_RCVTIMEO, (void *) &timeout, sizeof(timeout)) == -1) 
    {// SO_RCVTIMEO is used to set the time out value
     // SO_RCVTIMEO only accpets a timeval structure, so the if checks if it is valid
        cerr << "Cannot set socket options!\n";
        close(socket_desc);
        return 1;
    }

    bool flag = false;
	vector<double> coords = {};

    while (!flag) 
    {
		int input;
        
		while (coords.size() < 4)
		{   
       		//2. Read coordinates of start and end points from inpipe (blocks until they are selected)
			input = read(in, outbound, BUFFER_SIZE);

			if (input == 0) 
			{// Checking if no input from plotter    
				continue;
			}
			if (outbound[0] == 'Q') 
			{//Cheking if plotter tells to quit
				flag = true;
				string query = "Q";
				send(socket_desc, query.c_str(), query.size(), 0);
				break;
			}
			char buffer_in[24] = {0};
			int k = 0;
            
			for (int i = 0; i < BUFFER_SIZE; i++)
			{// Converts char array coordinates to vector double and then sorts
				if (outbound[i] == '\n' || outbound[i] == ' ')
				{
					buffer_in[k] = '\0';
					coords.push_back((double)atof(buffer_in));
					k = 0;
					continue;
				}
				if (outbound[i] == '\0') 
				{
					if (k != 0)
					{
						buffer_in[k] = '\0';
						coords.push_back((double)atof(buffer_in));
						k = 0;
					}
					break;
				}

				buffer_in[k] = outbound[i];
				k++;
			}
			memset(outbound, 0, BUFFER_SIZE);
			memset(buffer_in, 0, 24);
		}

		string parameter = "R ";
		//Create String to send to plotter starting with R
		parameter += to_string(static_cast<long long> (coords[0]*100000)) + " ";
		parameter += to_string(static_cast<long long> (coords[1]*100000)) + " ";
		parameter += to_string(static_cast<long long> (coords[2]*100000)) + " ";
		parameter += to_string(static_cast<long long> (coords[3]*100000));

        send(socket_desc, parameter.c_str(), parameter.size(), 0);

        int rec_size = recv(socket_desc, inbound, BUFFER_SIZE, 0); 
        //Receving first coordinates from server through socket
        
        if (rec_size == -1) 
        {//Check for timeout error if rec_size is -1 
            continue;
        }
       
      	//string val_n_str = (inbound + 2);
		//int value = stoi(val_n_str);
		int value = stoi(inbound + 2);

		if (value == 0)
		{// For N 0 case
			string initial = to_string(coords[0]) + " " + to_string(coords[1]) + '\n';
			initial += initial;
			initial += "E\n";
			// 3. Write to the socket
			write(out, initial.c_str(), initial.size());
			coords = {};
			continue;
		}
	    
		bool signal = true;
        vector<string> plotter_vector;

        while (true) 
        {// Sending coordinates to plotter in vector form
            
			string temp_storage;
			char packet[] = {'A'};
            //Packeting each node coordinates through sockets
            send(socket_desc, packet, strlen(packet) + 1, 0);
            memset(inbound, 0, BUFFER_SIZE);
            // 4. Read coordinates of waypoints one at a time
			int rec_size = recv(socket_desc, inbound, BUFFER_SIZE, 0); //Node Coordinated received
            
            if (rec_size == -1) 
            { //Check for timeout error if rec_size is -1
            	signal = false;
		break;
            }  

            if (strcmp("E", inbound) == 0) 
            {// Checking if the path ends and quit the loop
                string end = "E\n";
				plotter_vector.push_back(end);
				coords = {};
                break;
            }

            if (inbound[0] != 'W') 
            {// Checking for invalid response         
            	signal = false;
                cerr << "Invalid response from the server" << endl;
				break;
            }

            vector<long long> received_by_plotter;
            string path = "";

            for (int i = 2; i < BUFFER_SIZE; i++) 
            {// For W cases, converting received vector coordinates to string for the plotter to read
                if (inbound[i] == ' ') 
                {
                    received_by_plotter.push_back(stoll(path));// String to long long int
                    path = "";
                    continue;
                }
                if (inbound[i] == '\0') 
                {
                    received_by_plotter.push_back(stoll(path));// string to long long int
                    path = "";
                    break;
                } 
                path += inbound[i]; 
            }

            string plotter_coords = "";
            plotter_coords += (to_string((static_cast<double> (received_by_plotter[0]))/100000)) + " ";
            plotter_coords += (to_string((static_cast<double> (received_by_plotter[1]))/100000)) + "\n";

			if (value == 1)
			{// N 1 case
				plotter_coords += plotter_coords;
			}

            plotter_vector.push_back(plotter_coords);
        }

		if (signal)
		{
			for (int i = 0; i < plotter_vector.size(); i++)
			{   
                // 5. Write these coordinates to outpipe
				int bytes_written = write(out, plotter_vector[i].c_str(), plotter_vector[i].size());
                
                if (bytes_written == -1) 
                {
                    cerr << "Write operation failed!" << endl;
                }
			}
		}
    }

    // 7. Close the socket and pipes
    close(socket_desc);
    close(in);
    close(out);
    unlink(inpipe);
    unlink(outpipe);
    return 0;
}
