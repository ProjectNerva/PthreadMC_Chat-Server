#include <iostream>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <ctime>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <mutex>

//#define PORT 8081
#define MAX_LEN 200

// TODO: have another folder read all the online/live connections that can be connected. Displays ip and port #. Maybe security risk. 
// TODO: chat history txt in completely separate file
// TODO: display time for text

using namespace std;

struct clientInfo{
    int id;
    string name;
    int socket;
    pthread_t thread;
};

vector<clientInfo> clients;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER, cout_mutex = PTHREAD_MUTEX_INITIALIZER;
int seed = 0;

// function definition
void set_name(int id, char name[]);
void shared_print(string str, bool endLine); // synchronization of cout statements (?)
int broadcastMessage(string message, int sender_id);
int broadcastMessage(int num, int sender_id);
void endConnection(int id);
void *handleClient(void *arg);
void saveChatHistory(); // TODO
string getCurrentTime();

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cerr << "usage: port" <<  endl;
        exit(0);
    }

    int PORT = atoi(argv[1]);

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket == -1)
    {
        perror("socket");
        exit(-1);
    }
    else
    {
        cout << "Socket creation success..." << endl;
    }
    
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    serverAddress.sin_addr.s_addr = inet_addr("169.254.119.129");
    memset(&serverAddress.sin_zero, 0, sizeof(serverAddress.sin_zero));

    // bind the socket
    if ((bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1))
    {
        perror("bind error");
        exit(-1);
    }
    else
    {
        cout << "Bind success..." << endl;
    }

    // listening for connections
    if ((listen(serverSocket, 5)) == -1)
    {
        perror("listen error");
        exit(-1);
    }
    else
    {
        cout << "Listen success..." << endl;
    }

    cout << "Setup complete" << endl;

    sockaddr_in client;
    int clientSocket;
    unsigned int len = sizeof(sockaddr_in); // 16 (?/)

    cout << "Connected to chatroom!" << endl;
    cout << " ======== Welcome to Chatroom ======== " << endl;

    while(1)
    {
        // acceptibng the connection
        if ((clientSocket = accept(serverSocket, (struct sockaddr *)&client, &len)) == -1)
        {
            perror("accept error");
            exit(-1);
        }

        seed++;
        pthread_t clientThread;
        pthread_mutex_lock(&clients_mutex);
        // clientInfo clientData={seed, string("Anonymous"), clientSocket, clientThread};
        // clients.push_back( clientData );
        clients.push_back( (clientInfo){seed, string("Anonymous"), clientSocket, clientThread} );
        pthread_mutex_unlock(&clients_mutex);

        pthread_create(&clients.back().thread, NULL, handleClient, &clients.back());
    }

    close(serverSocket);
    return 0;
}

void set_name(int id, char name[])
{
    for (int i = 0; i < clients.size(); i++)
    {
        if (clients[i].id == id)
        {
            clients[i].name = string(name);
            break;
        }
    }
}

void shared_print(string str, bool endLine = true)
{
    pthread_mutex_lock(&cout_mutex);
    cout << str;
    if (endLine) cout << endl;
    pthread_mutex_unlock(&cout_mutex);
}

int broadcastMessage(string message, int sender_id)
{
    char temp[MAX_LEN];
    strcpy(temp, message.c_str());
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < clients.size(); i++)
    {
        if (clients[i].id != sender_id)
        {
            send(clients[i].socket, temp, sizeof(temp), 0);
        }
    }

    pthread_mutex_unlock(&clients_mutex);
    return 0;
}

int broadcastMessage(int num, int sender_id)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < clients.size(); i++)
    {
        if (clients[i].id != sender_id)
        {
            send(clients[i].socket, &num, sizeof(num), 0);
        }
    }

    pthread_mutex_unlock(&clients_mutex);
    return 0;
}

void endConnection(int id)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < clients.size(); i++)
    {
        if (clients[i].id == id)
        {
            close(clients[i].socket);
            pthread_cancel(clients[i].thread);
            clients.erase(clients.begin() + i);
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void *handleClient(void *arg)
{
    clientInfo *client = (clientInfo *)arg;
    int clientSocket = client->socket;
    int id = client->id;
    char name[MAX_LEN], buffer[MAX_LEN];

    // Receive and set client name
    recv(clientSocket, name, sizeof(name), 0);
    set_name(id, name);

    // Display welcome message
    string welcome_message = getCurrentTime() + " " + string(name) + " has joined";
    broadcastMessage(welcome_message, id);
    shared_print(welcome_message);

    while (true)
    {
        // Clear buffer and receive message
        memset(buffer, 0, sizeof(buffer)); // Clear buffer
        int bytes_received = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received <= 0 || strcmp(buffer, "#exit") == 0)
        {
            string leave_message = getCurrentTime() + " " + string(name) + " has left";
            broadcastMessage(leave_message, id);
            shared_print(leave_message);
            endConnection(id);
            break;
        }

        buffer[bytes_received] = '\0'; // Null-terminate the received string
        string message = getCurrentTime() + " " + string(name) + ": " + string(buffer);
        shared_print(message);
        broadcastMessage(message, id);
    }

    pthread_exit(NULL);
    return NULL;
}

void saveChatHistory()
{

}

string getCurrentTime()
{
    time_t now = time(nullptr);
    char buffer[64];
    strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S]", localtime(&now));
    return string(buffer);
}