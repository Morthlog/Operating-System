# Multithreaded Pizza Ordering & Delivery System (POSIX Threads)
This project was created to simulate a pizza ordering and delivery system using **POSIX threads (pthreads)**. The system uses **mutexes** and condition variables for mutual exclusion and thread synchronization in **C** to realistically model a busy pizza store with limited staff and resources. It was developed during the 4th semester as part of the Operating Systems course at AUEB.


## Task

The goal was to develop a **concurrent simulation** of a pizza store that handles a large number of customer orders, processed by a limited number of phone operators, cooks, ovens, and delivery drivers. The simulation emphasizes correct synchronization using:

* **Mutexes** (pthread_mutex_t) for safe access to shared data

* **Condition variables** (pthread_cond_t) for thread coordination (waiting and signaling)

## Architecture
Each order corresponds to a different thread that must be handled, which must pass the following critical points:
* Wait for a free phone operator

* Place an order (1–5 pizzas)

* Get charged (may fail)

* Wait for an available cook

* Wait for available ovens (one per pizza)

* Wait for a delivery driver

* Receive the order. 
  
The values of the [header](p3220162-p3220291-pizza.h) show the maximum allowed concurrency at each part of the system, as well as the *waiting time*.

Using **mutexes** and **conditional variables** we protect the shared resources, including printing on the screen.

We keep track of statistics, such as revenue and average waiting times. 

If any thread fails at any point, we make sure we free the resources it occupied using a custom **destructor**, including mutex locks using **pthread_cleanup_push/pop** to avoid deadlocks.

## How to run

### Prerequisites
* Your machine must be unix based, or support pthreads.

### Steps
* Download the repository
  
* In the project root type in the terminal `gcc -pthread p3220162-p3220291-pizza.c && ./a.out <orders_count> <seed>`
  
    * Uncomment #define EXTRA_DEBUG at the top of the .c file to include more detailed prints


## Contributors
<a href="https://github.com/Morthlog/Operating-System/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=Morthlog/Operating-System"/>
</a>

- [Babis Drosatos](https://github.com/BabisDros)
- [Iosif Petroulakis](https://github.com/Morthlog)
