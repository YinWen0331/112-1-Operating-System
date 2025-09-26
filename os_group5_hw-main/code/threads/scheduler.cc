// scheduler.cc 
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would 
//	end up calling FindNextToRun(), and that would put us in an 
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "scheduler.h"
#include "main.h"

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

Scheduler::Scheduler()
{ 
    L1ReadyList = new SortedList<Thread *>(BurstTimeCompare);
    L2ReadyList = new SortedList<Thread *>(PriorityCompare);          
    L3ReadyList = new List<Thread *>;

    toBeDestroyed = NULL;
} 

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{ 
    //delete readyList;
    delete L1ReadyList;
    delete L2ReadyList;                 
    delete L3ReadyList;
} 

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

/*
void
Scheduler::ReadyToRun (Thread *thread)
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
	//cout << "Putting thread on ready list: " << thread->getName() << endl ;
    thread->setStatus(READY);
    readyList->Append(thread);
}
*/

void Scheduler::ReadyToRun(Thread *thread)          
{
    int List;
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());

        if(thread->getPriority() >= 100 && thread->getPriority() <= 149){
            if( !kernel->scheduler->L1ReadyList->IsInList(thread) ){
                L1ReadyList->Insert(thread);
                List = 1;
            }
        }
        else if ( (thread->getPriority() >= 50 && thread->getPriority() <= 99) ){
            if( !L2ReadyList->IsInList(thread) ){
                L2ReadyList->Insert(thread);
                List = 2;
            }
        }
        else if ( (thread->getPriority() >= 0 && thread->getPriority() <= 49) ){
            if( !L3ReadyList->IsInList(thread) ){
                L3ReadyList->Append(thread);
                List = 3;
            }
        }
    

    DEBUG(dbgSche, "[A]Tick[" << kernel->stats->totalTicks << "]: Thread[" << thread->getID() 
        << "] is inserted into queue L[" << List << "]");     

    thread->setStatus(READY);
    thread->setPriority(thread->getInitPriority());
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun ()
{
    int List;
    Thread *nextThread;
    Thread *oldThread = kernel->currentThread;
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if( !L1ReadyList->IsEmpty() ){
        nextThread = L1ReadyList->RemoveFront();
        List = 1;
    }else if ( !L2ReadyList->IsEmpty() ){
        nextThread = L2ReadyList->RemoveFront();
        List = 2;
    }else if ( !L3ReadyList->IsEmpty() ){
        nextThread = L3ReadyList->RemoveFront();
        List = 3;
    }else {
        return NULL;
    }    
    
    DEBUG(dbgSche, "[B]Tick[" << kernel->stats->totalTicks 
            << "]: Thread[" << nextThread->getID() << "] is removed from queue L[" << List << "]");

    return nextThread;
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void
Scheduler::Run (Thread *nextThread, bool finishing)
{
    Thread *oldThread = kernel->currentThread;
    int oldThreadID;

    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {	// mark that we need to delete current thread
        ASSERT(toBeDestroyed == NULL);
	    toBeDestroyed = oldThread;
    }
    
    if (oldThread->space != NULL) {	// if this thread is a user program,
        oldThread->SaveUserState(); 	// save the user's CPU registers
	    oldThread->space->SaveState();
    }
    
    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow

    kernel->currentThread = nextThread;  // switch to the next thread
    nextThread->setStatus(RUNNING);      // nextThread is now running

    nextThread->setWaitingTime(0);
    
    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());
    
    // This is a machine-dependent assembly language routine defined 
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    DEBUG(dbgSche, "[E]Tick[" << kernel->stats->totalTicks << "]: Thread[" << nextThread->getID() <<
         "] is now selected for execution, thread[" << oldThread->getID() << "] is replaced, and it has executed [" <<
          oldThread->getExecutionTime() << "] ticks");     //12-7
    
    nextThread->setLastexecutionTime(kernel->stats->totalTicks);

    SWITCH(oldThread, nextThread);
    
    // we're back, running oldThread
    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed();		// check if thread we were running
					// before this one has finished
					// and needs to be cleaned up
    
    if (oldThread->space != NULL) {	    // if there is an address space
        oldThread->RestoreUserState();     // to restore, do it.
	    oldThread->space->RestoreState();
    }
    

    
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void
Scheduler::CheckToBeDestroyed()
{
    if (toBeDestroyed != NULL) {
        delete toBeDestroyed;
	    toBeDestroyed = NULL;
    }
}
 
//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
Scheduler::Print()
{
    cout << "L1ReadyList list contents:\n";
    L1ReadyList->Apply(ThreadPrint);
    cout << "\n";
    
}

void Scheduler::updatePriority()
{
    int oldPriority;
    int newPriority;

    ListIterator<Thread *> *iter1 = new ListIterator<Thread *>(L1ReadyList);
    ListIterator<Thread *> *iter2 = new ListIterator<Thread *>(L2ReadyList);
    ListIterator<Thread *> *iter3 = new ListIterator<Thread *>(L3ReadyList);

    Statistics *stats = kernel->stats;

    // L1
    int Tick = kernel->stats->totalTicks;
    for( ; !iter1->IsDone(); iter1->Next() ){
        ASSERT( iter1->Item()->getStatus() == READY);
        iter1->Item()->setWaitingTime(iter1->Item()->getWaitingTime()+TimerTicks);

        if(iter1->Item()->getWaitingTime() >= 1000 && iter1->Item()->getID() > 0 ){
            oldPriority = iter1->Item()->getPriority();
            newPriority = oldPriority + 10;
            //cout<<"L1\n";
            if (newPriority > 149){
                newPriority = 149;
            }
        iter1->Item()->setPriority(newPriority);
        iter1->Item()->setWaitingTime(0);
        DEBUG(dbgSche, "[C]Tick[" << Tick << "]: Thread [" << iter1->Item()->getID() << "] changes its priority from ["<<oldPriority<<"] to ["
            <<newPriority<<"]");
        }
    }

    // L2
    Tick = kernel->stats->totalTicks;
    for( ; !iter2->IsDone(); iter2->Next() ){
        ASSERT( iter2->Item()->getStatus() == READY);
        iter2->Item()->setWaitingTime(iter2->Item()->getWaitingTime()+TimerTicks);
        //cout<<iter2->Item()->getName()<<" is in L2 waiting time:"<<iter2->Item()->getWaitingTime()<<"\n";
        if(iter2->Item()->getWaitingTime() >= 1000 && iter2->Item()->getID() > 0 ){
            oldPriority = iter2->Item()->getPriority();
            newPriority = oldPriority + 10;
            //cout<<iter2->Item()->getName()<<" is in L2 update:"<<newPriority<<"\n";
            if (newPriority > 149){
                newPriority = 149;
            }
            iter2->Item()->setPriority(newPriority);
            iter2->Item()->setWaitingTime(0);
            DEBUG(dbgSche, "[C]Tick[" << Tick << "]: Thread [" << iter2->Item()->getID() << "] changes its priority from ["<<oldPriority<<"] to ["
            <<newPriority<<"]");
            
            /*
            if(newPriority >= 100){
                L2ReadyList->Remove(iter2->Item());
                ReadyToRun(iter2->Item());
            }
            */
        }
    }

    // L3
    Tick = kernel->stats->totalTicks;
    for( ; !iter3->IsDone(); iter3->Next() ){
        ASSERT( iter3->Item()->getStatus() == READY);
        iter3->Item()->setWaitingTime(iter3->Item()->getWaitingTime()+TimerTicks);
        //cout<<iter3->Item()->getName()<<" is in L3 waiting time:"<<iter3->Item()->getWaitingTime()<<"\n";
        if( iter3->Item()->getWaitingTime() >= 1000 && iter3->Item()->getID() > 0 ){
            oldPriority = iter3->Item()->getPriority();
            newPriority = oldPriority + 10;
            //cout<<iter3->Item()->getName()<<" is in L3 update:"<<newPriority<<"\n";
            if (newPriority > 149){
                newPriority = 149;
            }
            iter3->Item()->setPriority(newPriority);
            iter3->Item()->setWaitingTime(0);
            DEBUG(dbgSche, "[C]Tick[" << Tick << "]: Thread [" << iter3->Item()->getID() << "] changes its priority from ["<<oldPriority<<"] to ["
            <<newPriority<<"]");
            /*
            if(newPriority >= 50){
                L3ReadyList->Remove(iter3->Item());
                ReadyToRun(iter3->Item());
            }
            */

        }
    }
}

int Scheduler::checkRemain(){
    Thread *thread = kernel->currentThread;
    ListIterator<Thread *> *iter1 = new ListIterator<Thread *>(L1ReadyList);

    if( !L1ReadyList->IsEmpty() ){
        if(iter1->Item()->getRemain() < thread->getRemain()){
            return 1;
        }
    }
    return -1;
}

int BurstTimeCompare(Thread* a, Thread* b){
    a->setRemain(a->getBurstTime() - a->getTotalExe());
    b->setRemain(b->getBurstTime() - b->getTotalExe());

    if(a->getRemain() != b->getRemain()){
        if (a->getRemain() < b->getRemain()){
            return -1;
        }
        else{
            return 1;
        }
    }else{
        return a->getID() < b->getID() ? -1 : 1;
    }
    return 0;
}
int PriorityCompare(Thread *a, Thread *b) {
    if(a->getPriority() != b->getPriority()){
        return a->getPriority() > b->getPriority() ? -1 : 1;
    }/*
    else{
        return a->getID() < b->getID() ? -1 : 1;
    }*/

    return 0;
}