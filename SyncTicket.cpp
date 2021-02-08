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

#define BUFFER_SIZE 100
#define MAX_ITEMS 10

typedef struct clientInfo* buffer_item;
int START_NUMBER = 0;
buffer_item buffer[BUFFER_SIZE];

pthread_mutex_t mutex;
sem_t empty;
sem_t full;

int insertPointer = 0, removePointer = 0;
bool *reservations;

void* client(void* param);
void* teller(void* param);

int main() {
    int numOfClientThreads, numOfTellerThreads;
    pthread_mutex_init(&mutex, NULL);
    sem_init(&full, 0, 0);
    sem_init(&empty,0,BUFFER_SIZE);
    vector<clientInfo*> clientInfos;
    cout << "Welcome to the Sync-Ticket!" << endl;

    ifstream configFile("configuration_file.txt");
    string line, theatreName, seats;

    if(!configFile.good()) {
        cout << "Error" <<endl;
        return 0;
    }
    else {
        getline(configFile, theatreName);
        getline(configFile, seats);
        cout << "Theatre Name: " << theatreName << endl;
        cout << "Number of Seats: " << seats << endl;
        reservations = new bool[stoi(seats)];

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

    numOfClientThreads = clientInfos.size(); // TODO: .size() ClientInfos.size()
    pthread_t pid[numOfClientThreads];

    numOfTellerThreads = 3;
    pthread_t cid[numOfClientThreads];

    //Create producer and consumer threads
    for(int i = 0; i<numOfClientThreads; i++) {
        pthread_create(&pid[i], NULL, &client, clientInfos[i]); //TODO: pass ClientInfo
    }
    for(int j = 0; j<numOfTellerThreads; j++) {
        pthread_create(&cid[j], NULL, &teller, clientInfos[j]);
    }
    //Join threads
    for(int i = 0; i < numOfClientThreads; i++) {
        pthread_join(pid[i], NULL);
    }
    for(int j = 0; j < numOfTellerThreads; j++) {
        pthread_join(cid[j], NULL);
    }

    pthread_mutex_destroy(&mutex);
    sem_destroy(&full);
    sem_destroy(&empty);
    return 0;
}

void* client(void* param) {
    buffer_item item = new clientInfo;
    for(int i = 0; i < MAX_ITEMS; i++) {

        clientInfo *client = (clientInfo *) param;
        sleep(2);
        sem_wait(&empty);

        //Critical section
        pthread_mutex_lock(&mutex);
        item->clientName = "CLIENT_" + to_string(START_NUMBER++);
        buffer[insertPointer] = item;
        insertPointer = (insertPointer + 1) % BUFFER_SIZE;
        printf("Client %u produced %s \n", (unsigned int)pthread_self(), item->clientName.c_str());
        pthread_mutex_unlock(&mutex);
        sem_post(&full);
    }
    pthread_exit(NULL);
}

void* teller(void* param) {
    buffer_item item;
    for(int i = 0; i < MAX_ITEMS; i++) {
        sleep(2);
        sem_wait(&full);

        //Critical section
        pthread_mutex_lock(&mutex);
        item = buffer[removePointer];
        removePointer = (removePointer + 1) % BUFFER_SIZE;
        printf("Teller %u consumed %s \n", (unsigned int)pthread_self(), item->clientName.c_str());
        pthread_mutex_unlock(&mutex);

    }
}

