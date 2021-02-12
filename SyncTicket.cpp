#include <fstream>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sstream>
#include <pthread.h>
#include <cstring>
#include <string>
#include <semaphore.h>

using namespace std;

struct clientInfo {
    string clientName;
    int arrivalTime, serviceTime, seatNumber;
};

#define BUFFER_SIZE 3
#define MAX_ITEMS 1000

typedef struct clientInfo* buffer_item;
int START_NUMBER = 0;
buffer_item buffer[BUFFER_SIZE];

pthread_mutex_t queue_mutex, reservationMutex;
sem_t empty;
sem_t full;

int insertPointer = 0, removePointer = 0, theatreCapacity;
bool *reservations;

void* client(void* param);
void* teller(void* param);
string tellerNames[3] = {"A","B","C"};
ofstream out;
string configuration_path, output_path;

int main(int argc, char* argv[]) {
    configuration_path = string(argv[1]);
    output_path = string(argv[2]);

    int numOfClientThreads, numOfTellerThreads;
    pthread_mutex_init(&queue_mutex, NULL);
    pthread_mutex_init(&reservationMutex, NULL);
    sem_init(&full, 0, 0);
    sem_init(&empty,0,BUFFER_SIZE);
    vector<clientInfo*> clientInfos;

    out.open(output_path);
    out << "Welcome to the Sync-Ticket!" << endl;
    cout << "Welcome to the Sync-Ticket!" << endl; //TODO: Delete
    ifstream configFile(configuration_path);
    string line, theatreName, seats;

    if(!configFile.good()) {
        out << "Error reading the file" << endl;
        return 0;
    }
    else {
        getline(configFile, theatreName);
        getline(configFile, seats);
        cout << "Theatre Name: " << theatreName << endl;
        cout << "Number of Seats: " << seats << endl;
        theatreCapacity = stoi(seats);
        reservations = new bool[theatreCapacity+1];

        while (getline(configFile,line)) {
            stringstream ss(line);
            string delimiter = ",";
            string token;
            size_t pos = 0;
            vector<string> parts;

            while((pos = line.find(delimiter)) !=string::npos) {
                token = line.substr(0, line.find(delimiter));
                parts.push_back(token);
                line.erase(0, pos + delimiter.length());
            }
            parts.push_back(line);

            clientInfo* info = new clientInfo;
            info->clientName = parts[0];
            info->arrivalTime = stoi(parts[1]);
            info->serviceTime = stoi(parts[2]);
            info->seatNumber = stoi(parts[3]);

            clientInfos.push_back(info);
        }
    }

    cout << clientInfos.size() << endl;

    numOfClientThreads = clientInfos.size();
    pthread_t pid[numOfClientThreads];

    numOfTellerThreads = 3;
    pthread_t cid[numOfClientThreads];

    //Create producer and consumer threads
    for(int j = 0; j<numOfTellerThreads; j++) {
        pthread_create(&cid[j], NULL, &teller, &(tellerNames[j]));
    }

    for(int i = 0; i<numOfClientThreads; i++) {
        pthread_create(&pid[i], NULL, &client, clientInfos[i]);
    }

    //Join threads
    for(int i = 0; i < numOfClientThreads; i++) {
        pthread_join(pid[i], NULL);
    }
    for(int j = 0; j < numOfTellerThreads; j++) {
        pthread_join(cid[j], NULL);
    }


    out.close();
    pthread_mutex_destroy(&queue_mutex);
    pthread_mutex_destroy(&reservationMutex);
    sem_destroy(&full);
    sem_destroy(&empty);
    delete [] reservations;
    return 0;
}

void* client(void* param) {
    buffer_item item = (clientInfo *) param;
    usleep(item->arrivalTime * 1000);
    sem_wait(&empty);

    /*
     * Critical section for the producer buffer operations
     */
    pthread_mutex_lock(&queue_mutex);
    buffer[insertPointer] = item;
    insertPointer = (insertPointer + 1) % BUFFER_SIZE;
    //printf("Client %u produced %d %s\n", (unsigned int)pthread_self(), item->arrivalTime, item->clientName.c_str());
    pthread_mutex_unlock(&queue_mutex);

    sem_post(&full);

    pthread_exit(NULL);
}

void* teller(void* param) {
    const char* tellerName = (*((string*)param)).c_str();
    out << "Teller " << tellerName << " has arrived." << endl;
    printf("Teller %s has arrived.\n",tellerName); //TODO: Delete
    buffer_item item;
    for(int i = 0; i < MAX_ITEMS; i++) {
        int givenSeat;
        sem_wait(&full);

        /*
         * Critical section for consumer buffer operations
         */
        pthread_mutex_lock(&queue_mutex);
        item = buffer[removePointer];
        removePointer = (removePointer + 1) % BUFFER_SIZE;
        pthread_mutex_unlock(&queue_mutex);

        /*
         * Critical section for the reservations
         */
        pthread_mutex_lock(&reservationMutex);
        if (reservations[item->seatNumber]) {
            /*
             * If the seat is full, gives the lowest numbered available seat
             */
            int x;
            for (x = 1; x < theatreCapacity+1; x++) {
                if(!reservations[x]) {
                    reservations[x] = true;
                    givenSeat = x;
                    break;
                }
            }
            if (x==theatreCapacity+1) {
                givenSeat = -1;
            }
        } else {
            /*
             * If the seat is empty, gives the requested seat to the client
             */
            reservations[item->seatNumber] = true;
            givenSeat = item->seatNumber;
        }
        pthread_mutex_unlock(&reservationMutex);

        /*
         * Prints the lines after the service time as requested by the project output
         */
        usleep(item->serviceTime*1000);
        if(givenSeat>0) {
            out << item->clientName.c_str() << " requests seat " << item->seatNumber << ", reserves seat " << givenSeat << ". Signed by Teller " << tellerName << "." << endl;
            printf("%s requests seat %d, reserves seat %d. Signed by Teller %s\n",item->clientName.c_str(), item->seatNumber, givenSeat, tellerName); //TODO: Delete
        } else {
            out << item->clientName.c_str() << " requests seat " << item->seatNumber << " reserves seat None. Signed by Teller " << tellerName << "." << endl;
            printf("%s requests seat %d, reserves seat None. Signed by Teller %s\n", item->clientName.c_str(), item->seatNumber, tellerName); //TODO: Delete
        }
        sem_post(&empty);
    }
    pthread_exit(NULL);
}
