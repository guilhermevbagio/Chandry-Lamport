        #include <stdio.h>
        #include <stdlib.h>
        #include <string.h>
        #include <mpi.h>
        #include <unistd.h>
        #include <semaphore.h>
        #include <pthread.h>
        #include <time.h>
        
        #define TAMANHO_FILA 5
        
        
        #define VALOR_SNAPSHOT 3
        #define PROCESSO_INICIALIZADOR 0
        
        typedef struct Clock {
            int p[3];
        } Clock;
        
        Clock relogioGlobal;
        
        int SNAPSHOT_RESERVED_PROCESS, LAST_MESSAGING_PROCESS;
        
        const int path_size = 7;
        
        int path1 [] = {1,   2,   1,   3,   1,   2,   1};
        int path2 [] = {1, -99, -99, -99, -99, -99, -99};
        int path3 [] = {3,   1, -99, -99, -99, -99, -99};
        
        //-99 HERE USED AS FLAG FOR NULL

        int* paths[] = {path1, path2, path3};
        int* mypath;
        int currentPosition = 0;
        
        int my_rank, comm_size;
        
        typedef struct {
            Clock data[TAMANHO_FILA];
            int front;
            int rear;
            int count;
            sem_t mutex;
            sem_t empty;
            sem_t full;
        } Fila;
        
        typedef struct Snapshot {
            Fila filas[2];
            Clock clock;
            int rank;
        } Snapshot;
        
        
        typedef struct{
            Fila* entrada;
            Fila* saida;
        } Filas;
        
        Clock maxClock(Clock clock1, Clock clock2) {
            Clock result;
            for (int i = 0; i <= 3; ++i) {
                result.p[i] = (clock1.p[i] > clock2.p[i]) ? clock1.p[i] : clock2.p[i];
            }
            return result;
        }
        
        void initFila(Fila *q) {
            q->front = 0;
            q->rear = -1;
            q->count = 0;
            sem_init(&q->mutex, 0, 1);
            sem_init(&q->empty, 0, TAMANHO_FILA);
            sem_init(&q->full, 0, 0);
        
            for (int i = 0; i < TAMANHO_FILA; ++i) {
                for (int j = 0; j < 3; ++j) {
                    q->data[i].p[j] = 0;
                }
            }
        }
        
        void printFila(const Fila *fila) {

            printf("Front: %d; ", fila->front);
            printf(" Rear: %d; ", fila->rear);
            printf(" Count: %d; \n", fila->count);

            for (int i = 0; i < TAMANHO_FILA; i++) {
                
                printf("Slot %d: [%d, %d, %d]\n", i, fila->data[i].p[0], fila->data[i].p[1], fila->data[i].p[2]);
            }

            
            printf("\n");
            
        }
        
        void incrementaRelogio(Clock *relogio){
            relogio->p[my_rank] ++;
        }
        
        void inverteRelogio(Clock *relogio){
            
            for(int i = 0; i < 3; ++i){
                relogio->p[i] *= -1;
            }
        }
        
        
        void Enfileira(Fila *q, Clock item) {
    
            q->rear = (q->rear + 1) % TAMANHO_FILA;
            q->data[q->rear] = item;
            q->count++;
        
            sem_post(&q->mutex);
        }
        
        Clock Desenfileira(Fila *q) {
    
            Clock item = q->data[q->front];
            q->front = (q->front + 1) % TAMANHO_FILA;
            q->count--;
        
            sem_post(&q->mutex);
        
            return item;
        }
    
        void printSnapshot(Snapshot snapshot) {
            printf(
                "\n"
                "                     > Snapshot for Process %d <\n"
                "                     > Clock: (%d, %d, %d)\n",
                snapshot.rank, snapshot.clock.p[0], snapshot.clock.p[1], snapshot.clock.p[2]);
            
            printf("Fila de entrada: \n");
            printFila(&snapshot.filas[0]);
            printf("Fila de saida: \n");
            printFila(&snapshot.filas[1]);
        }
        
        void printAllSnapshots(Snapshot snapshots[]) {

            printf("\nC-ALL--SNAP-SNAP------------------------------------SNAP-SNAP--ALL-C\n");
            
            for (int i = 0; i <= LAST_MESSAGING_PROCESS; ++i) {
                printSnapshot(snapshots[i]);
            }
            
            printf("\nC-SNAP-SNAP-SNAP------------------------------------SNAP-SNAP-SNAP-C\n");

        }



        
        void snapshotProcess() {
            Snapshot snapshots[LAST_MESSAGING_PROCESS + 1];
        
            for (int i = 0; i <= LAST_MESSAGING_PROCESS; ++i) {
                MPI_Recv(&snapshots[i], sizeof(Snapshot), MPI_BYTE, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        
            sleep(2);
        
            printAllSnapshots(snapshots);
        }
        
        
        
        
        
        void takeSnapshot(Filas* filas_processo){
            Snapshot snapshotLocal;
        
            snapshotLocal.clock.p[0] = relogioGlobal.p[0];
            snapshotLocal.clock.p[1] = relogioGlobal.p[1];
            snapshotLocal.clock.p[2] = relogioGlobal.p[2];
            
            for (int i = 0; i < TAMANHO_FILA; ++i) {
                snapshotLocal.filas[0].data[i] = filas_processo->entrada->data[i];
                snapshotLocal.filas[1].data[i] = filas_processo->saida->data[i];
            }
            
            snapshotLocal.filas[0].front = filas_processo->entrada->front;
            snapshotLocal.filas[0].rear = filas_processo->entrada->rear;
            snapshotLocal.filas[0].count = filas_processo->entrada->count;
            
            
            snapshotLocal.filas[1].front = filas_processo->saida->front;
            snapshotLocal.filas[1].rear = filas_processo->saida->rear;
            snapshotLocal.filas[1].count = filas_processo->saida->count;
            
            snapshotLocal.rank = my_rank;
            
            MPI_Send(&snapshotLocal, sizeof(Snapshot), MPI_BYTE, SNAPSHOT_RESERVED_PROCESS, 0, MPI_COMM_WORLD); // Corrected the size
            printf("Processo %d enviou snapshot \n", my_rank);
        }


        
        
        void* entryQueueHandler(void* arg) {
            
            Filas* filas_processo = (Filas*)arg;
            Fila* entrada = filas_processo->entrada;
            
            Enfileira(entrada, relogioGlobal);
            sem_post(&filas_processo->entrada->full);
            
            while(currentPosition < path_size)
            {
                
                Clock relogio;
                
                
                MPI_Recv(&relogio, sizeof(Clock), MPI_BYTE, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                
                
                //Nao printar receive de marker de snapshot
                if(relogio.p[0] < 0){
                    
                    
                    Enfileira(entrada, relogio);
                    sem_post(&filas_processo->entrada->full);
                    
                    continue;
                }
                printf(
                    "\n"
                    "X-RECV-RECV-RECV------------------------------------RECV-RECV-RECV-X\n"
                    "               > Process %d received clock <\n"
                    "               > Clock: (%d, %d, %d)\n"    
                    ">------------------------------------------------------------------<\n\n", 
                    my_rank, relogio.p[0], relogio.p[1], relogio.p[2]);
    
                
                Enfileira(entrada, relogio);
                sem_post(&filas_processo->entrada->full);

            }
            return NULL;
        }
        
        void* blackBox(void* arg){
            
            Filas* filas_processo = (Filas*)arg;

            while(currentPosition < path_size)
            {
                
            
                Clock relogio;
                
                // espera ter algo na fila de entrada
                sem_wait(&filas_processo->entrada->full);
                
                // retira o relogio da fila de entrada
                relogio = Desenfileira(filas_processo->entrada);
                
                // comunica a retirada da fila de entrada
                sem_post(&filas_processo->entrada->empty);
                
                
                //se o relogio for menor que 0, submete snapshot
                if(relogio.p[0] < 0 || relogio.p[1] < 0 || relogio.p[2] < 0)
                {

                    if(my_rank == PROCESSO_INICIALIZADOR) 
                    {   
                        continue;
                    }
                    
                    takeSnapshot(filas_processo);
                    
                    
                    

                    // espera caber na fila de saida
                    sem_wait(&filas_processo->saida->empty);
                    
                    // insere e comunica
                    Enfileira(filas_processo->saida, relogio);
                    sem_post(&filas_processo->saida->full);

                    continue;
                
                }
                
                relogioGlobal = maxClock(relogio, relogioGlobal);  
                
                
                //COMPORTAMENTO PADRAO -> FORA DE SNAPSHOT
                incrementaRelogio(&relogioGlobal);
                    
                
                relogio = relogioGlobal;  
                    
                    /*
                    printf("\n"
                        "+-INCR-MENT-CLCK------------------------------------INCR-MENT-CLCK-+\n"
                        "               > Process %d incremented its clock                  \n"
                        "               > Clock: (%d, %d, %d)\n"    
                        "+--------------------------------++++------------------------------+\n\n", 
                        my_rank, relogioGlobal.p[0], relogioGlobal.p[1], relogioGlobal.p[2]);
                    */
                   
                //AQUI É INSERIDO UM MARCADOR DE SNAPSHOT
                //inicia snapshot a cada ciclo
                if(relogio.p[PROCESSO_INICIALIZADOR] == VALOR_SNAPSHOT && my_rank == PROCESSO_INICIALIZADOR){
                    
                    takeSnapshot(filas_processo);
                    inverteRelogio(&relogio);
                    
                }
                
                // espera caber na fila de saida
                sem_wait(&filas_processo->saida->empty);
                
                
                // insere e comunica
                Enfileira(filas_processo->saida, relogio);
                sem_post(&filas_processo->saida->full);
                
                sleep(1);
                
            }
            return NULL;
            
        }
        
        
        void* exitQueueHandler(void* arg) {
            
            Filas* filas_processo = (Filas*)arg;
            Fila* saida = filas_processo->saida;
            
            while(currentPosition < path_size)
            {
                
                //espera ter algo na fila de saida
                sem_wait(&filas_processo->saida->full);
                
                //retira e manda
                Clock relogio = Desenfileira(saida);
                
                if(relogio.p[0] < 0){
                    MPI_Send(&relogio, sizeof(Clock), MPI_BYTE, (my_rank + 1) % SNAPSHOT_RESERVED_PROCESS, 0, MPI_COMM_WORLD);
                    continue;
                }
                
                int next_send = mypath[currentPosition] - 1;
                
                if(next_send == -100){
                    sleep(2);
                    continue;
                }
                
                MPI_Send(&relogio, sizeof(Clock), MPI_BYTE, next_send, 0, MPI_COMM_WORLD);
                printf(
                    "\n"
                    "X-SEND-SEND-SEND------------------------------------SEND-SEND-SEND-X\n"
                    "               > Process %d sent clock to process %d. <\n"
                    "               > Clock: (%d, %d, %d)\n"    
                    ">------------------------------------------------------------------<\n\n", 
                    my_rank, next_send, relogio.p[0], relogio.p[1], relogio.p[2]);
                
                sleep(1);
                currentPosition ++;
            }
            return NULL;
        }
    
        
    
    
        



        
        
        void process() {
            
            Clock relogioGlobal;
            relogioGlobal.p[0] = 0;
            relogioGlobal.p[1] = 0;
            relogioGlobal.p[2] = 0;

            
            pthread_t entry_thread, exit_thread, black_box_thread;
            
            Filas filas_processo;
        
            filas_processo.entrada = (Fila*)malloc(sizeof(Fila));
            filas_processo.saida = (Fila*)malloc(sizeof(Fila));
            
            mypath = paths[my_rank];
            
            
            initFila(filas_processo.entrada);
            initFila(filas_processo.saida);
        
            pthread_create(&entry_thread, NULL, entryQueueHandler, &filas_processo);
            pthread_create(&exit_thread, NULL, exitQueueHandler, &filas_processo);
            pthread_create(&black_box_thread, NULL, blackBox, &filas_processo);
            
            
            
            
            printf(
                        "\n"
                        "X-INIT-INIT-INIT------------------------------------INIT-INIT-INIT-X\n"
                        "\n"
                        "                      Process %d initialized.\n"
                        "                        Clock: (%d, %d, %d)\n"
                        "\n"
                        ">-----------------------------XXXXXXXX-----------------------------<\n"
                        "\n",
                        my_rank, relogioGlobal.p[0], relogioGlobal.p[1], relogioGlobal.p[2]);
        
            
            
            
            pthread_join(black_box_thread, NULL);
            pthread_join(entry_thread, NULL);
            pthread_join(exit_thread, NULL);
        
            free(filas_processo.entrada);
            free(filas_processo.saida);
            
            
        }
    
        
        
    void printHeader() {
        
        if (my_rank == 0) {
            printf(
                "\n"
                "X-MARK-PROG-INIT------------------------------------MARK-PROG-INIT-X\n"
                "\n"
                "                        >PROGRAM INITIALIZED<                         "
                "\n"
                ">------------------------------<><.><>-----------------------------<\n");
        }
        if(my_rank == SNAPSHOT_RESERVED_PROCESS){
            printf("\n                      *system is snapshot ready*\n");
        }
        
    }
    
    int main(int argc, char *argv[]) {
        
        MPI_Init(&argc, &argv);
        MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
        MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
        
        SNAPSHOT_RESERVED_PROCESS = comm_size - 1;
        LAST_MESSAGING_PROCESS = SNAPSHOT_RESERVED_PROCESS - 1;
        
        printHeader();
        
        //garante a impressao do header antes de continuar
        MPI_Barrier(MPI_COMM_WORLD);
        
        //o ultimo processo é o responsavel pelo snapshot
        if (my_rank == SNAPSHOT_RESERVED_PROCESS){
            
            snapshotProcess();
        }
        else process();
        
        MPI_Finalize();
        return 0;
    }
        
