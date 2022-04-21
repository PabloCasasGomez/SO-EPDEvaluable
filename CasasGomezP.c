#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <limits.h> // for INT_MAX, INT_MIN

#define NUMPLANTAS 10
#define NUMASCENSORES 2
#define TIEMPOMOVIMIENTOASCENSOR 1
#define MAXPERSONAS 500 // Podemos modificar el tamanio de las colas de plantas

typedef struct
{
    int buf[MAXPERSONAS];
    long head, tail;
    int full, empty;
} queue;

// Variables globales

// Array de colas por planta
queue *colasPlantas[NUMPLANTAS];
queue *colaGlobal; // Cola global para mantener orden de personas

// Array de semaforos binarios para pulsadores y ascensores
// De esta forma inicializamos los semaforos para todo el array
pthread_mutex_t pulsadores[NUMPLANTAS];

// Con este semaforo binario hago que solo uno pueda actuar al mismo tiempo
pthread_mutex_t turnoAscensor;

// Array con las posiciones de los ascensores, con esta forma se inicia ambos en la posicion 0.
// Si queremos hacer que empiecen en posiciones aleatorias tenemos funcion random
int plantasAscensores[NUMASCENSORES] = {0};

// Funciones globales
queue *queueInit(void);
void queueAdd(queue *q, int in);
void queueDel(queue *q);
void queueDelete(queue *q);
void *controlPersonas(void *variables);
int posicionCola(int indentificadorPersona);
void *controlAscensores(void *identificador);
void unlink_previous_semaphores();

int main(int argc, char *argv[])
{

    // Deslinkeamos los semaforos pervios
    pthread_mutex_destroy(&turnoAscensor);

    for (int i = 0; i < NUMPLANTAS; i++)
    {
        pthread_mutex_destroy(&pulsadores[i]);
    }

    // Tenemos que inicializarlos ahora
    pthread_mutex_init(&turnoAscensor, NULL);

    for (int i = 0; i < NUMPLANTAS; i++)
    {
        pthread_mutex_init(&pulsadores[i], NULL);
    }

    printf("Se ha eliminado cualquier previo existente\n");
    // Validaciones everywhere!!
    char *valorIntroducido;
    int NUMPERSONAS;
    if (argc > 1)
    {

        long conv = strtol(argv[1], &valorIntroducido, 10);

        // Comprobamos que lo que se ha metido no es un String o se pasa del valor de int
        if (*valorIntroducido != '\0' || conv > INT_MAX || conv < INT_MIN)
        {
            printf("ERROR no ha introducido un numero");
            return 0;
        }
        else
        {
            // No error pero comprobamos que el valor sea siempre mayor que 0
            NUMPERSONAS = conv;
            while (NUMPERSONAS <= 0)
            {
                printf("\nSolo valores mayores de 0!");
                printf("\nIntroduce un valor mayor que 0: ");
                scanf("%d", &NUMPERSONAS);
            }
            printf("El NUM PERSONAS ES %d\n", NUMPERSONAS);
        }
    }
    else
    {
        printf("\nNo ha introducido ningun valor para NUMPERSONAS:");
        printf("\nIntroduce un valor mayor que 0");
        scanf("%d", &NUMPERSONAS);
        while (NUMPERSONAS <= 0)
        {
            printf("\nSolo valores mayores de 0!");
            printf("\nIntroduce un valor mayor que 0: ");
            scanf("%d", &NUMPERSONAS);
        }
        printf("\nEL NUM PERSONA ES %d\n", NUMPERSONAS);
    }

    // // Introduce el numero de usuarios podríamos
    // int NUMPERSONAS = -1;

    // // Validacion de las que te gustan Aurelio
    // while (NUMPERSONAS <= 0)
    // {
    //     printf("\nIntroduzca el numero de personas: ");
    //     scanf("%d", &NUMPERSONAS);

    //     if (NUMPERSONAS == 0)
    //     {
    //         printf("\nIntroduce un numero de personas mayor de 0.\n");
    //     }
    // }

    // Posicion de las personas tiene que ponerse aqui dentro porque NUMPERSONAS esta introducido por teclado
    int posicionPersonas[NUMPERSONAS];

    // Creamos un array de personas con NUMPERSONAS
    pthread_t personas[NUMPERSONAS];

    // Creamos un array de personas con NUMPERSONAS se podría crear fuera
    pthread_t ascensores[NUMASCENSORES];

    // Creamos las colas de cada planta de momento vacio (Esto es NUMPLANTAS pero lo pongo asi para ser mas guay)
    for (int i = 0; i < (sizeof(colasPlantas) / sizeof(colasPlantas[0])); i++)
    {
        colasPlantas[i] = queueInit(); // Lo dice una EPD
    }

    colaGlobal = queueInit(); // Lo dice una EPD

    // Introducimos esto para que cada vez que se ejecute el programa el random sea diferente
    srand(getpid());

    // Inicializamos las PERSONAS en posiciones random (Pista son hilos)
    printf("\nInicializacion de personas: \n");

    // Bucles para crear hilos ya que en mi caso personas = hilos
    for (int i = 0; i < NUMPERSONAS; i++)
    {

        // Creamos *identificador y hacemos un malloc para optimizar
        int *identificadorPersona = malloc(sizeof(*identificadorPersona));

        // Hacemos que *identificador valga i para pasarlo a la hora de crear el thread y de esta forma eliminamos el problema de los hilos
        *identificadorPersona = i;

        // Array con los parametros necesarios para que funcione controlPersonas HACEN FALTA 2 PARAMETROS
        // Primera variable es identificador de la persona en cuestion, segundo es el array con las posiciones de las personas
        void *variables[2] = {identificadorPersona, posicionPersonas};

        // Creacion de hilos
        if (pthread_create(&personas[i], NULL, controlPersonas, variables) != 0)
        {
            perror("pthread_create\n");
            exit(-1);
        }

        // Para que no haya problema enseñando el mensaje por pantalla
        sleep(1);
    }

    // Bucle para iniciar los ascensores
    for (int i = 0; i < NUMASCENSORES; i++)
    {

        // Creamos *identificador y hacemos un malloc para optimizar
        int *identificadorAscensor = malloc(sizeof(*identificadorAscensor));

        // Hacemos que *identificador valga i para pasarlo a la hora de crear el thread y de esta forma eliminamos el problema de los hilos
        *identificadorAscensor = i;

        // Creacion de hilos
        void *variables[2] = {identificadorAscensor, posicionPersonas};
        if (pthread_create(&ascensores[i], NULL, controlAscensores, variables) != 0)
        {
            perror("pthread_create\n");
            exit(-1);
        }
        sleep(0.2);
        // Para que no haya problema enseñando el mensaje por pantalla
        // sleep(1);
    }

    // Tenemos que usar un join para los hilos creados
    for (int j = 0; j < NUMPERSONAS; j++)
    {
        if (pthread_join(personas[j], NULL) != 0)
        {
            perror("pthread_join");
        }
    }

    // Tenemos que usar un join para los hilos creados
    for (int j = 0; j < NUMASCENSORES; j++)
    {
        if (pthread_join(ascensores[j], NULL) != 0)
        {
            perror("pthread_join");
        }
    }

    // // Destruimos los dos semaforos binarios
    pthread_mutex_destroy(pulsadores);
    pthread_mutex_destroy(&turnoAscensor);

    return 0;
}

// Asignacion aleatoria de personas
void *controlPersonas(void *variables)
{
    // Variables pasadas por parametro
    void **data = (void **)variables;

    // Identificador de personas
    int identificadorPersona = *(int *)data[0];

    // Array con las posiciones de las personas
    int *posicionPersonas = (int *)data[1];

    // Creamos posiciones de plantas aleatorias
    int plantaRandom = rand() % NUMPLANTAS;

    posicionPersonas[identificadorPersona] = plantaRandom;
    printf("\nLa persona %d esta en la planta %d\n", identificadorPersona, plantaRandom);

    // Como estoy tocando una variable global desde varios hilos hago un block del array de cola global
    // Bloqueo el pulsador de mi planta hasta que termine todo mi proceso
    pthread_mutex_lock(&pulsadores[posicionPersonas[identificadorPersona]]);

    // Incluyo cada persona en su cola de planta correspondiente
    // Ademas de esta forma solo estoy permitiendo que se puedan incluir en aquellas plantas donde no haya gente previamente
    queueAdd(colasPlantas[plantaRandom], identificadorPersona);
    queueAdd(colaGlobal, identificadorPersona);

    printf("\nLa persona %d ha pulsado el llamador de la Planta %d", identificadorPersona, posicionPersonas[identificadorPersona]);
    // printf(", podemos ver como ahora la head vale %lu y la tail tiene un valor de %lu", colasPlantas[plantaRandom]->head, colasPlantas[plantaRandom]->tail);

    // Cerramos hilo
    pthread_exit(NULL);
}

void *controlAscensores(void *identificador)
{
    // Variables pasadas por parametro
    void **data = (void **)identificador;

    // Identificador de ascensor
    int identificadorAscensor = *(int *)data[0];

    // Array posiciones Personas
    int *posicionPersonas = (int *)data[1];

    // Indicamos que se ha creado el ascensor con identificador X
    printf("\nIdentificador de ascensor %d\n", identificadorAscensor);
    // printf("La Primera persona es %d",colaGlobal->buf[colaGlobal->head]);

    // Comprueban que no quede nadie por atender
    // Si queremos que el programa no acabe podemos poner tan solo || fin=0
    int fin = -1;
    int soloImprimeUnaVez = 0;
    while (!colaGlobal->empty || fin == 0)
    {
        if (!colaGlobal->empty)
        {
            //     printf("Quedan personas por atender");
            //     sleep(0.5);
            // }

            // Hago un bloqueo de la cola para que el otro ascensor no interfiera
            pthread_mutex_lock(&turnoAscensor);
            // printf("SOY EL ASCENSOR %d Y ESTOY ACTUANDO",identificadorAscensor);

            int persona = colaGlobal->buf[colaGlobal->head];
            int plantaPersona = posicionPersonas[colaGlobal->buf[colaGlobal->head]];

            printf("\nSoy el ascensor %d y VOY a la Planta %d a recoger a la Persona %d", identificadorAscensor, plantaPersona, persona);
            plantasAscensores[identificadorAscensor] = plantaPersona;

            // Libero el pulsador de la planta de la Persona que va la primera
            pthread_mutex_unlock(&pulsadores[posicionPersonas[colaGlobal->buf[colaGlobal->head]]]);

            // Eliminamos a la persona de la planta en cuestion
            // Eliminamos a la persona de la cola global
            // El ascensor se dirije a la planta de la Persona
            queueDel(colasPlantas[posicionPersonas[colaGlobal->buf[colaGlobal->head]]]);
            queueDel(colaGlobal);

            // Libero al otro ascensor para que el pueda hacer sus operaciones
            pthread_mutex_unlock(&turnoAscensor);

            // Plantas de diferencia entre persona y ascensor
            int diferenciaPlantas = abs(plantasAscensores[identificadorAscensor] - plantaPersona);

            sleep(TIEMPOMOVIMIENTOASCENSOR * diferenciaPlantas);

            // Calculo una nueva planta para la persona recogida
            int nuevaPlantaRandom = rand() % NUMPLANTAS;

            // Validacion de la que te gusta Aurelio para que no haya opciones de que se quiera ir a la misma planta
            while (nuevaPlantaRandom == plantaPersona)
            {
                nuevaPlantaRandom = rand() % NUMPLANTAS;
            }

            diferenciaPlantas = abs(plantaPersona - nuevaPlantaRandom);

            printf("\nEl ascensor %d se DIRIGE a la nueva planta %d de la Persona %d", identificadorAscensor, nuevaPlantaRandom, persona);
            sleep(diferenciaPlantas * TIEMPOMOVIMIENTOASCENSOR);

            // if (!colaGlobal->empty)
            // {
            // Indico cual es la siguiente persona a recoger y su planta
            // printf("\nSiguiente Persona a recoger es la Persona %d en la planta %d", colaGlobal->buf[colaGlobal->head], posicionPersonas[colaGlobal->buf[colaGlobal->head]]);
        }
        if (colaGlobal->empty && soloImprimeUnaVez == 0)
        {
            printf("\nNo quedan Personas disponibles para el ascensor %d\n", identificadorAscensor);
            soloImprimeUnaVez++;
            sleep(TIEMPOMOVIMIENTOASCENSOR);
        }
    }
}

// No hace falta que sea una funcion pero queda menos complicada la linea de consulta
int posicionCola(int indentificadorPersona)
{
    return colasPlantas[indentificadorPersona]->buf[colasPlantas[indentificadorPersona]->head];
}

// Inicializamos las colas
queue *queueInit(void)
{
    queue *q;
    q = (queue *)malloc(sizeof(queue));
    if (q == NULL)
        return (NULL);
    q->empty = 1;
    q->full = 0;
    q->head = 0;
    q->tail = 0;
    return (q);
}

// Elimina la cola
void queueDelete(queue *q)
{
    free(q);
}

// Anade un elemento a la cola
void queueAdd(queue *q, int in)
{
    q->buf[q->tail] = in;
    q->tail++;
    if (q->tail == MAXPERSONAS)
        q->tail = 0;
    if (q->tail == q->head)
        q->full = 1;
    q->empty = 0;
    return;
}

// Eliminacion de elementos en la cola
void queueDel(queue *q)
{
    // Movemos la cabeza hacia la derecha al sacar el primer elemento de la cola
    q->head++;

    // Mismo comportamiento que ejercicios de prática
    if (q->head == sizeof(q->buf))
        q->head = 0;
    if (q->head == q->tail)
        q->empty = 1;
    q->full = 0;
    return;
}