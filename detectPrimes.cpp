// =============================================================================
// You must modify this file and then submit it for grading to D2L.
// =============================================================================

/* File Name: detectPrimes.cpp
*  Assignment: Assignment 3
*  Submission Date: November 11, 2021
*  Author: Chad Holst
*  UCID: 30105378
*  Course: CPSC 457
*  Instructor: Pavol Federl
*
*  This program was created with the guidance of the Assignment 3 document and pseudocode provided by Pavol Federl; CPSC 457 lecture notes and tutorials.
*/

#include "detectPrimes.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <queue>
#include <atomic>
#include <stdio.h>
#include <iostream>

void checkSimpleNumber(); // prototypes of new functions
void parallelizedWhileLoop(int64_t ind);

int64_t checkPrime, interval; // globals for threads to share
double sqrRootNum;
std::vector<int64_t> result;
std::queue<int64_t> inputQueue;
std::atomic<bool> divisorFound (false);
std::atomic<bool> noNumbsLeft (false);
int64_t threadSize = 0;
pthread_barrier_t myBarrier;

struct Task // struct that contains each thread 
{
    pthread_t tid; // thread id
    int64_t index;
};

// returns true if n is prime, otherwise returns false
// -----------------------------------------------------------------------------
// to get full credit for this assignment, you will need to adjust or even
// re-write the code in this function to make it multithreaded.
static bool is_prime(int64_t n)
{
  // handle trivial cases
  if (n < 2) return false;
  if (n <= 3) return true; // 2 and 3 are primes
  if (n % 2 == 0) return false; // handle multiples of 2
  if (n % 3 == 0) return false; // handle multiples of 3
  // try to divide n by every number 5 .. sqrt(n)
  int64_t i = 5;
  int64_t max = sqrt(n);
  while (i <= max) 
  {
    if (n % i == 0) return false; // thread cancellation goes here
    if (n % (i + 2) == 0) return false;
    i += 6;
  }
  // didn't find any divisors, so it must be a prime
  return true;
}

void parallelizedWhileLoop(int64_t ind) // Note: All primes greater than 3 can be written as 6k +- 1 where k is an int and k > 0
{
    int64_t index = ind; // index of each parallel thread
    int64_t i = interval*index + 5; // start of interval: multiple of 6 times Task[i].tid number + 5 (offset)
    int64_t end = i + interval; // end of interval

    while(i <= end && !(divisorFound.load())) // parallelize inner while loop in is_prime() for each parallel thread range
    {
        if(divisorFound.load()) break; // cancel threads so they stop trying to find divisor together
        if ((checkPrime % i == 0) || (checkPrime % (i + 2) == 0))
        {
            divisorFound.store(true, std::memory_order_release); // set flag to cancel parallel threads
            break;
        }
        i += 6;
    }
    
}

void checkSimpleNumber() // use old function for small numbers 
{
    std::atomic<bool> check = is_prime(checkPrime);
    if(check.load())
    {
        result.push_back(checkPrime);
        divisorFound.store(true, std::memory_order_release); // to skip parallel computing & adding the same result we use one flag
    }
    divisorFound.store(true, std::memory_order_release); // skip parallel section
}

void* threadTask(void* arg)
{
    struct Task *data = (struct Task*)arg; // unpack parameter for thread index
    int64_t index = data -> index;
    while(!(noNumbsLeft.load())) // while there are numbers left in queue
    {
        if(pthread_barrier_wait(&myBarrier) != 0) // before serial thread which checks for next numb & prepares work for parallel
        {
            if(inputQueue.empty() == false)
            {
                checkPrime = inputQueue.front();
                inputQueue.pop(); // take out number from queue
            }
            if(inputQueue.empty() == true) // no numbers left, compute this number first and check next serial thread
            {
                noNumbsLeft.store(true, std::memory_order_release); // global to check flag if no numbers are left
            }
            if(checkPrime <= 9999) // check lower numbers
            {
                checkSimpleNumber(); // use single threaded solution
            }
            else // check for certain cases and maybe divide work into equal intervals 
            {
                sqrRootNum = sqrt(checkPrime); // square root number for while loop
                if(((checkPrime % (int64_t)sqrRootNum == 0) || (checkPrime % (int64_t)sqrRootNum + 2) == 0)) // check if sqrRootnum divides
                {
                    divisorFound.store(true, std::memory_order_release); // set global flag for divisors
                }
                else if((checkPrime % 2 == 0) || (checkPrime % 3 == 0)) // multiples of 2 or 3 
                {
                    divisorFound.store(true, std::memory_order_release); 
                }
                else // divide work by making intervals for 5 ---> sqrt(checkPrime)
                {
                    double difference = sqrRootNum - 5; // prepare for parallelized while loop algorithm
                    int64_t ratio = round(difference/threadSize); // round ratio up or down 
                    interval =  ratio + (6 - ratio % 6); // round interval to the next integer multiple of 6
                }
            }
        }

        pthread_barrier_wait(&myBarrier); // parallel task executed by all threads where they compute their intervals
        if(!(divisorFound.load())) // check if divisor was found before calling parallelized while loop
        {
            parallelizedWhileLoop(index); // call the parallelized while loop for each thread to compute its interval
        }

        if(pthread_barrier_wait(&myBarrier) != 0) // is serial after thread
        {
            if(!divisorFound.load()) // if no divisor, then must be prime
            {
                result.push_back(checkPrime);
            }
            divisorFound.store(false, std::memory_order_release); // reset flag to false
        }
        if(noNumbsLeft.load()) pthread_exit(0); // after computing, check if no numbers left flag was set to true
    } 
    pthread_exit(0);
}

// This function takes a list of numbers in nums[] and returns only numbers that
// are primes.
//
// The parameter n_threads indicates how many threads should be created to speed
// up the computation.
// -----------------------------------------------------------------------------
// You will most likely need to re-implement this function entirely.
// Note that the current implementation ignores n_threads. Your multithreaded
// implementation must use this parameter.
std::vector<int64_t>
detect_primes(const std::vector<int64_t> &nums, int n_threads)
{
    if(n_threads < 1 || n_threads > 256) // ensure we are within assumption range
    {
        printf("Error: invalid amount of threads");
        exit(1);
    }
    else if(n_threads == 1) // use single-threaded solution to find primes
    {
        for (auto num : nums) 
        {
            if (is_prime(num)) result.push_back(num); 
        }
        return result;       
    }
    else // use multi-threaded solution to find primes
    {
        for(size_t i = 0; i < nums.size(); i++) // put nums in queue for evaluation
        {
            if(nums.at(i) >= 2) // if input is greater than or equal to 2 then add to queue
            {
                inputQueue.push(nums.at(i)); // queue will be used to check primes
            }
        }

        threadSize = n_threads; // initialize globals for threadTask():
        pthread_barrier_init(&myBarrier, NULL, threadSize); // initialize barrier

        Task tasks[threadSize]; // create threads using a struct for barrier & threadTask():
        for(int64_t i = 0; i < threadSize; i++)
        {
            tasks[i].index = i; // assign an index to each thread in the Task struct
            pthread_create(&tasks[i].tid, NULL, threadTask, &tasks[i]);
        }
        
        for(int i = 0; i < threadSize; i++)
        {
            pthread_join(tasks[i].tid, 0);
        }

        pthread_barrier_destroy(&myBarrier); 

        return result;
    }
}