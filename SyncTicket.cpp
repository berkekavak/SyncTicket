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

struct tellerInfo {
    string tellerName;
    int tellerNo;
};

#define NUMBER_OF_TELLERS 3

typedef struct clientInfo* buffer_item;
buffer_item buffer[NUMBER_OF_TELLERS];

pthread_mutex_t queue_mutex, reservationMutex, tellerMutex;
sem_t empty;
sem_t full;

sem_t jobReady[3];
sem_t resultReady[3];

int theatreCapacity;
bool *reservations;

void* client(void* param);
void* teller(void* param);
string tellerNames[3] = {"A","B","C"};
ofstream out;
string configuration_path, output_path;
bool isTellerBusy[3];
clientInfo* jobs[3];
int numOfClientThreads, numOfTellerThreads;
bool running = true;

int main(int argc, char* argv[]) {
    configuration_path = string(argv[1]);
    output_path = string(argv[2]);

    pthread_mutex_init(&queue_mutex, NULL);
    pthread_mutex_init(&reservationMutex, NULL);
    pthread_mutex_init(&tellerMutex, NULL);
    sem_init(&full, 0, 0);
    for (int i = 0; i < 3; i++) {
        sem_init(&(jobReady[i]), 0, 0);
        sem_init(&(resultReady[i]), 0, 0);
    }
    sem_init(&empty, 0, NUMBER_OF_TELLERS); // teller count
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
        tellerInfo* info = new tellerInfo;
        info->tellerName = tellerNames[j];
        info->tellerNo = j;
        pthread_mutex_lock(&tellerMutex);
        usleep(1000);
        pthread_create(&cid[j], NULL, &teller, info);
        usleep(1000);
        pthread_mutex_unlock(&tellerMutex);

    }

    for(int i = 0; i<numOfClientThreads; i++) {
        pthread_create(&pid[i], NULL, &client, clientInfos[i]);
    }

    //Join threads
    for(int i = 0; i < numOfClientThreads; i++) {
        pthread_join(pid[i], NULL);
    }

    out << "All clients received service." << endl;

    out.close();
    pthread_mutex_destroy(&queue_mutex);
    pthread_mutex_destroy(&reservationMutex);
    pthread_mutex_destroy(&tellerMutex);
    sem_destroy(&full);
    sem_destroy(&empty);
    for (int i = 0; i < 3; i++) {
        sem_destroy(&(jobReady[i]));
        sem_destroy(&(resultReady[i]));
    }
    delete [] reservations;
    return 0;
}

void* client(void* param) {
    buffer_item item = (clientInfo *) param;
    usleep(item->arrivalTime * 1000);

    sem_wait(&empty); //
    // if we're here, one of the tellers should be available

    pthread_mutex_lock(&queue_mutex);
    int tellerNo;
    for (int i = 0; i < 3; i++) {
        if (!isTellerBusy[i]) {
            tellerNo = i;
            jobs[i] = item;
            sem_post(&(jobReady[i])); // inc
            break;
        }
    }
    pthread_mutex_unlock(&queue_mutex);

    //printf("client %s waiting for teller %d...\n", item->clientName.c_str(), tellerNo);
    sem_wait(&(resultReady[tellerNo]));
    delete item;
    pthread_exit(NULL);
}

void* teller(void* param) {
    tellerInfo* info = (tellerInfo*)param;
    out << "Teller " << info->tellerName.c_str() << " has arrived." << endl;
    printf("Teller %s has arrived.\n",info->tellerName.c_str()); //TODO: Delete
    buffer_item item;
    while(running) {
        int givenSeat;
        sem_wait(&(jobReady[info->tellerNo]));
        isTellerBusy[info->tellerNo] = true;
        item = jobs[info->tellerNo];

        /*
         * Critical section for the reservations
         */
        pthread_mutex_lock(&reservationMutex);

        if (item->seatNumber > theatreCapacity || reservations[item->seatNumber]) {
            /*
             * If the seat number is too high or seat is full, gives the lowest numbered available seat
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
        }
        else {
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
            out << item->clientName.c_str() << " requests seat " << item->seatNumber << ", reserves seat " << givenSeat << ". Signed by Teller " << info->tellerName.c_str() << "." << endl;
            printf("%s requests seat %d, reserves seat %d. Signed by Teller %s\n",item->clientName.c_str(), item->seatNumber, givenSeat, info->tellerName.c_str()); //TODO: Delete
        } else {
            out << item->clientName.c_str() << " requests seat " << item->seatNumber << " reserves seat None. Signed by Teller " << info->tellerName.c_str() << "." << endl;
            printf("%s requests seat %d, reserves seat None. Signed by Teller %s\n", item->clientName.c_str(), item->seatNumber, info->tellerName.c_str()); //TODO: Delete
        }

        isTellerBusy[info->tellerNo] = false;
        sem_post(&(resultReady[info->tellerNo])); // inc
        sem_post(&empty);
    }
    pthread_exit(NULL);
}
