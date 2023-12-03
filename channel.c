#include "channel.h"


typedef struct NotifierStruct {                                     
  int notificationFlag;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} Notifier;

// typedef struct NotifierStruct Notifier;                   // Typedef for Notifier node
  
// static void set_signal_flag(Notifier* NotifierPTR) {    // Set signal flag for Notifier node
//     pthread_mutex_lock(&NotifierPTR->mutex);              // Lock Notifier node mutex
//     NotifierPTR->notificationFlag = 1;                          // Set signal flag to 1
//     pthread_mutex_unlock(&NotifierPTR->mutex);            // Unlock Notifier node mutex
// }

// static void signal_condition(Notifier* NotifierPTR) {       // Signal Notifier condition variable
//     pthread_mutex_lock(&NotifierPTR->mutex);                  //  Lock Notifier node mutex
//     pthread_cond_signal(&NotifierPTR->cond);                  //  Signal Notifier condition variable
//     pthread_mutex_unlock(&NotifierPTR->mutex);                //  Unlock Notifier node mutex
// }

static void SignalNotifier(void* data) {                     // Inform (unblock) a Notifier
    Notifier* NotifierPTR = (Notifier*)data;             // Cast data to Notifier node
    pthread_mutex_lock(&NotifierPTR->mutex);              // Lock Notifier node mutex
    NotifierPTR->notificationFlag = 1;                          // Set signal flag to 1
        pthread_cond_signal(&NotifierPTR->cond);                  //  Signal Notifier condition variable
    pthread_mutex_unlock(&NotifierPTR->mutex);            // Unlock Notifier node mutex
}


// static void Notifier_wait(Notifier *NotifierPTR) {
 
//   pthread_mutex_lock(&NotifierPTR->mutex);                         // Lock the mutex    

//   // Wait until an event on list
//   // do {
//   //   pthread_cond_wait(&NotifierPTR->cond, &NotifierPTR->mutex); }  // Wait on condition variable
//    while(NotifierPTR->notificationFlag == 0){
//     pthread_cond_wait(&NotifierPTR->cond, &NotifierPTR->mutex);
//   }                            

//   pthread_mutex_unlock(&NotifierPTR->mutex);                       // Unlock the mutex
// }

// static Notifier* Notifier_create(select_t *channel_list, size_t channel_count) {
   
//     Notifier *NotifierPTR = (Notifier*) malloc(sizeof(Notifier));      // Allocate memory for Notifier node
//     if (NotifierPTR == NULL) return NULL;                                              // Return NULL if allocation fails  

//     if (pthread_mutex_init(&NotifierPTR->mutex, NULL) != 0) {                   // Initialize mutex
//         free(NotifierPTR);                                                      // Free Notifier node if mutex initialization fails
//         return NULL;                                                              // Return NULL if mutex initialization fails
//     }
//     if (pthread_cond_init(&NotifierPTR->cond, NULL) != 0) {                     // Initialize condition variable
//         pthread_mutex_destroy(&NotifierPTR->mutex);                             // Destroy mutex if condition variable initialization fails
//         free(NotifierPTR);                                                      // Free Notifier node if condition variable initialization fails
//         return NULL;
//     }

//   // Iterate over each channel

//     for (size_t i = 0; i < channel_count; ++i) {                      
//         select_t *selector = &channel_list[i];
//         if (pthread_mutex_lock(&selector->channel->mutex) != 0) continue;              // Lock the mutex before modifying the channel
//         list_insert(selector->channel->Notifiers, (void*)NotifierPTR);             // Insert Notifier node into the list
//         pthread_mutex_unlock(&selector->channel->mutex);                               // Unlock the mutex after modifying the channel
//     }

//     return NotifierPTR;
// }

// static void Notifier_destroy(select_t * channel_list, size_t channel_count, Notifier * NotifierPTR){
  
//   for(size_t i = 0; i < channel_count; ++i){
//     select_t * selector = &channel_list[i];
//     pthread_mutex_lock(&selector->channel->mutex);                               //  Lock the mutex before modifying the channel
//     list_node_t* node = list_find(selector->channel->Notifiers, (void*)NotifierPTR);     // Find Notifier node in the list
    
//     if(node != NULL){                                                       // If Notifier node is found in the list
//       list_remove(selector->channel->Notifiers, node);                         // Remove Notifier node from the list
//       free(node);
//     }
    
//     pthread_mutex_unlock(&selector->channel->mutex);                             // Unlock the mutex after modifying the channel
//   }
  
//   pthread_mutex_destroy(&NotifierPTR->mutex);                           //  Destroy mutex
//   pthread_cond_destroy(&NotifierPTR->cond);                             //  Destroy condition variable
//   free(NotifierPTR);                                                    //  Free Notifier node    
// }

// Creates a new channel with the provided size and returns it to the caller
// A 0 size indicates an unbuffered channel, whereas a positive size indicates a buffered channel
chan_t* channel_create(size_t size)
{
    if (size == 0) return NULL;                         // Return NULL if size is 0
    chan_t* channel = (chan_t*) calloc(1, sizeof(chan_t));  // Allocate memory for the channel
    
    
    if (channel == NULL) return NULL;                   // Return NULL if memory allocation failed




    while(1){
        if ((channel->buffer = buffer_create(size)) == NULL) break;  // Create a buffer for the channel

        if (pthread_mutex_init(&channel->mutex, NULL) != 0) break;   // Initialize the mutex for the channel

        if (pthread_cond_init(&channel->cond, NULL) != 0) break;    // Initialize the condition variable for the channel

        if ((channel->Notifiers = list_create()) == NULL) break;  //  Create a list of Notifiers for the channel

        return channel;                                             // Return the channel if all the above steps were successful
    }
   

    if (channel->buffer != NULL) free(channel->buffer);             // Free the buffer if it was created
    if (channel->Notifiers != NULL) free(channel->Notifiers);   // Free the list of Notifiers if it was created
    pthread_mutex_destroy(&channel->mutex);                         // Destroy the mutex if it was initialized
    pthread_cond_destroy(&channel->cond);                           // Destroy the condition variable if it was initialized
    free(channel);                                                  // Free the channel

    return NULL;                                                    // Return NULL if any of the above steps failed
}

// Writes data to the given channel
// This can be both a blocking call i.e., the function only returns on a successful completion of send (blocking = true), and
// a non-blocking call i.e., the function simply returns if the channel is full (blocking = false)
// In case of the blocking call when the channel is full, the function waits till the channel has space to write the new data
// Returns SUCCESS for successfully writing data to the channel,
// WOULDBLOCK if the channel is full and the data was not added to the buffer (non-blocking calls only),
// CLOSED_ERROR if the channel is closed, and
// OTHER_ERROR on encountering any other generic error of any sort
enum chan_status channel_send(chan_t* channel, void* data, bool blocking)
{
    // Attempt to lock the channel. On failure, return general error
        if(pthread_mutex_lock(&channel->mutex) != 0){                       // lock the channel
            return OTHER_ERROR;                                               // return error if lock failed
        }

if (blocking != true) // its a non blocking call 
    {
          

        // If the channel is closed, unlock the mutex and return closed error
        if(channel->end_flag){                                              // check if the channel is closed
            pthread_mutex_unlock(&channel->mutex);                          // unlock the mutex before signalling the Notifiers
            return CLOSED_ERROR;                                            // return error, if channel is closed
        }

        // If the buffer is full, unlock the mutex and return channel full error
        if(buffer_current_size(channel->buffer) == buffer_capacity(channel->buffer)){  // check if the buffer is full
            pthread_mutex_unlock(&channel->mutex);                                     // unlock the mutex before signalling the Notifiers
            return WOULDBLOCK;                                                       // return error, if channel is full
        }

        // Try to add data to buffer, on failure unlock mutex and return general error
        if(buffer_add(data,channel->buffer) != true){                       // add data to channel
            pthread_mutex_unlock(&channel->mutex);                                     // unlock the mutex before signalling the Notifiers
            return OTHER_ERROR;                                                          // return error, if add failed
        }

        // If data added successfully, broadcast condition variable and signal Notifiers
        pthread_cond_broadcast(&channel->cond);                             // tell waiting threads data was inserted
        list_foreach(channel->Notifiers, SignalNotifier);               // tell each Notifier we have event on this list

        // Finally unlock the mutex and return success
        pthread_mutex_unlock(&channel->mutex);                              // unlock the mutex before signalling the Notifiers

        return SUCCESS;
    }
    else{
        //  if(pthread_mutex_lock(&channel->mutex) != 0) return OTHER_ERROR;

        while(buffer_current_size(channel->buffer) == buffer_capacity(channel->buffer)){ //wait while the buffer is full
            if(channel->end_flag){
                pthread_mutex_unlock(&channel->mutex); // unlock the mutex before signalling the Notifiers
                return CLOSED_ERROR;
            }
            pthread_cond_wait(&channel->cond, &channel->mutex); //wait for a signal that the buffer is not full
        }

        if(channel->end_flag){                             //check if the channel is closed
            pthread_mutex_unlock(&channel->mutex);         //unlock the mutex before signalling the Notifiers
            return CLOSED_ERROR;                           //return error, if channel is closed
        }
        
        if(buffer_add(data,channel->buffer) != true){ //add data to channel
            pthread_mutex_unlock(&channel->mutex);               //unlock the mutex before signalling the Notifiers
            return OTHER_ERROR;                                    //return error, if add failed      
        }

        pthread_cond_broadcast(&channel->cond);                  //tell waiting threads data was inserted

        pthread_mutex_unlock(&channel->mutex);                   // unlock the mutex before signalling the Notifiers

        list_foreach(channel->Notifiers, SignalNotifier);   // tell each Notifier we have event on this list
        
        return SUCCESS;
    }   
    return SUCCESS;
}






// Reads data from the given channel and stores it in the function’s input parameter, data (Note that it is a double pointer).
// This can be both a blocking call i.e., the function only returns on a successful completion of receive (blocking = true), and
// a non-blocking call i.e., the function simply returns if the channel is empty (blocking = false)
// In case of the blocking call when the channel is empty, the function waits till the channel has some data to read
// Returns SUCCESS for successful retrieval of data,
// WOULDBLOCK if the channel is empty and nothing was stored in data (non-blocking calls only),
// CLOSED_ERROR if the channel is closed, and
// OTHER_ERROR on encountering any other generic error of any sort
enum chan_status channel_receive(chan_t* channel, void** data, bool blocking)
{
  if (pthread_mutex_lock(&channel->mutex) != 0){                                                          // Lock the mutex
        return OTHER_ERROR;
    }
if(blocking == true){
  
    if(channel->end_flag){                                     // check if the channel is closed
      pthread_mutex_unlock(&channel->mutex);
      return CLOSED_ERROR;                                       // return error, if channel is closed
    }

    //wait while the buffer is empty
    while(  (buffer_current_size(channel->buffer) == 0)  )   // check if the buffer is empty // check if the channel is closed
    {                        
      
      pthread_cond_wait(&channel->cond, &channel->mutex);      // wait for a signal that the buffer is not empty
    }

    if(channel->end_flag){                                     // check if the channel is closed
      pthread_mutex_unlock(&channel->mutex);
      return CLOSED_ERROR;                                       // return error, if channel is closed
    }

    *data = buffer_remove(channel->buffer);                      //get data from channel
    if(*data == NULL){                                           //return error, if remove failed
      pthread_mutex_unlock(&channel->mutex);
      return OTHER_ERROR;
    }

    pthread_cond_broadcast(&channel->cond);                    // tell waiting threads data was removed

    list_foreach(channel->Notifiers, SignalNotifier);      // tell each Notifier we have event on this list

                                                    

  pthread_mutex_unlock(&channel->mutex);                       // unlock the mutex before signalling the Notifiers

  return SUCCESS;                                                  // return the status
  } 

  else{
    if(channel->end_flag){
           
        pthread_mutex_unlock(&channel->mutex);                                                       // Unlock the mutex
        return CLOSED_ERROR;                                                         // Return error if channel is closed
    } else if(buffer_current_size(channel->buffer) == 0){
                                                            //  Return error if channel is empty
                pthread_mutex_unlock(&channel->mutex);                                                       // Unlock the mutex
              return WOULDBLOCK;                                                         // Return error if channel is closed

    } else {
        *data = buffer_remove(channel->buffer);                           // Remove data from buffer
        if (*data == NULL) {
            
            pthread_mutex_unlock(&channel->mutex);                                                       // Unlock the mutex
            return OTHER_ERROR;                                                         // Return error if channel is closed
                                                // Return error if data was not removed successfully
        } else {                                                                                           // If data was removed successfully 
                               // Handle success
            pthread_cond_broadcast(&channel->cond);                                                                           // Broadcast the condition variable
            list_foreach(channel->Notifiers, SignalNotifier);                                                            // Signal the Notifiers
            pthread_mutex_unlock(&channel->mutex);                                                                            // Unlock the mutex
            return SUCCESS;                                                                                         // Return success

        }
    }
    // return SUCCESS;
  }
  // return SUCCESS;
}









// Closes the channel and informs all the blocking send/receive/select calls to return with CLOSED_ERROR
// Once the channel is closed, send/receive/select operations will cease to function and just return CLOSED_ERROR
// Returns SUCCESS if close is successful,
// CLOSED_ERROR if the channel is already closed, and
// OTHER_ERROR in any other error case
enum chan_status channel_close(chan_t* channel)
{
    if (pthread_mutex_lock(&channel->mutex) != 0) {                                                 // Lock the mutex
        return OTHER_ERROR;                                                                           // Return error if lock failed
    }

    if (channel->end_flag == 1) {                                                                   // Check if the channel is already closed
        pthread_mutex_unlock(&channel->mutex);                                                      // Unlock the mutex
        return CLOSED_ERROR;                                                                        // Return error if channel is already closed
    }

    channel->end_flag = 1;                                                                          // Set the end flag to 1

    list_foreach(channel->Notifiers, SignalNotifier);                                           // Signal the Notifiers

    pthread_cond_broadcast(&channel->cond);                                                         // Broadcast the condition variable

    pthread_mutex_unlock(&channel->mutex);                                                          // Unlock the mutex

    return SUCCESS;
}

// Frees all the memory allocated to the channel
// The caller is responsible for calling channel_close and waiting for all threads to finish their tasks before calling channel_destroy
// Returns SUCCESS if destroy is successful,
// DESTROY_ERROR if channel_destroy is called on an open channel, and
// OTHER_ERROR in any other error case
enum chan_status channel_destroy(chan_t* channel)
{
    bool channel_not_closed;                                                                // Boolean to check if the channel is closed
    
    pthread_mutex_lock(&channel->mutex);                                                    // Lock the mutex
    channel_not_closed = (channel->end_flag == 0);                                          // Check if the channel is closed
    pthread_mutex_unlock(&channel->mutex);                                                  // Unlock the mutex

    if (channel_not_closed) {                                                               // Return error if channel is not closed
        return DESTROY_ERROR;                                                               // Return error if channel is not closed
    }

    // cleanup synchronization primitives
    pthread_cond_destroy(&channel->cond);                                                   // Destroy the condition variable
    pthread_mutex_destroy(&channel->mutex);                                                 // Destroy the mutex

    // free resources associated with the channel
    buffer_free(channel->buffer);                                                           //  Free the buffer
    list_destroy(channel->Notifiers);                                                     // Destroy the list of Notifiers
    
    // deallocate the channel
    free(channel);                                                                          // Free the channel

    return SUCCESS;
}

// Takes an array of channels, channel_list, of type select_t and the array length, channel_count, as inputs
// This API iterates over the provided list and finds the set of possible channels which can be used to invoke the required operation (send or receive) specified in select_t
// If multiple options are available, it selects the first option and performs its corresponding action
// If no channel is available, the call is blocked and waits till it finds a channel which supports its required operation
// Once an operation has been successfully performed, select should set selected_index to the index of the channel that performed the operation and then return SUCCESS
// In the event that a channel is closed or encounters any error, the error should be propagated and returned through select
// Additionally, selected_index is set to the index of the channel that generated the error
enum chan_status channel_select(size_t channel_count, select_t* channel_list, size_t* selected_index)
{
    size_t i;
    Notifier* subnode = NULL;
    enum chan_status rv = SUCCESS;

    //set selected index to invalid value, so we know operation is not performed
    *selected_index = channel_count;

    //loop until we get an event on any channel
    while(1){

      //first try to send without blocking
      for(i=0; i < channel_count; i++){

        select_t * sel = &channel_list[i];
        //try to do the operation on this list
        rv = (sel->is_send == true) ? channel_send(sel->channel, sel->data, false)
                                : channel_receive(sel->channel, &sel->data, false);
        //in case message was sent or channel closed
        if((rv == SUCCESS) || (rv == CLOSED_ERROR)){  //on GEN_ERROR too ?
          *selected_index = i;
          break;
        }
      }

      //if no operation performed

      if(*selected_index == channel_count){

        if(subnode == NULL){  //if we are not subscribed to events
          // subnode = Notifier_create(channel_list, channel_count);
          Notifier *NotifierPTR = (Notifier*) malloc(sizeof(Notifier));      // Allocate memory for Notifier node
          if (NotifierPTR == NULL) return WOULDBLOCK;                                              // Return NULL if allocation fails  

    if (pthread_mutex_init(&NotifierPTR->mutex, NULL) != 0) {                   // Initialize mutex
        free(NotifierPTR);                                                      // Free Notifier node if mutex initialization fails
        return WOULDBLOCK;                                                              // Return NULL if mutex initialization fails
    }
    if (pthread_cond_init(&NotifierPTR->cond, NULL) != 0) {                     // Initialize condition variable
        pthread_mutex_destroy(&NotifierPTR->mutex);                             // Destroy mutex if condition variable initialization fails
        free(NotifierPTR);                                                      // Free Notifier node if condition variable initialization fails
        return WOULDBLOCK;
    }

  // Iterate over each channel

    for (size_t i = 0; i < channel_count; ++i) {                      
        select_t *selector = &channel_list[i];
        if (pthread_mutex_lock(&selector->channel->mutex) != 0) continue;              // Lock the mutex before modifying the channel
        list_insert(selector->channel->Notifiers, (void*)NotifierPTR);             // Insert Notifier node into the list
        pthread_mutex_unlock(&selector->channel->mutex);                               // Unlock the mutex after modifying the channel
    }
        }else{
          
          // Notifier_wait(subnode); //wait for Notifier event
          pthread_mutex_lock(&subnode->mutex);                         // Lock the mutex    

          // Wait until an event on list
          while(subnode->notificationFlag == 0){
            pthread_cond_wait(&subnode->cond, &subnode->mutex);
          }                            

          pthread_mutex_unlock(&subnode->mutex);                       // Unlock the mutex
        }
      }else{
        //we have an event, stop loop
        break;
      }
    }

    if(subnode){
      // Notifier_destroy(channel_list, channel_count, subnode);
      for(size_t i = 0; i < channel_count; ++i){
    select_t * selector = &channel_list[i];
    pthread_mutex_lock(&selector->channel->mutex);                               //  Lock the mutex before modifying the channel
    list_node_t* node = list_find(selector->channel->Notifiers, (void*)subnode);     // Find Notifier node in the list
    
    if(node != NULL){                                                       // If Notifier node is found in the list
      list_remove(selector->channel->Notifiers, node);                         // Remove Notifier node from the list
      free(node);
    }
    
    pthread_mutex_unlock(&selector->channel->mutex);                             // Unlock the mutex after modifying the channel
  }
  
  pthread_mutex_destroy(&subnode->mutex);                           //  Destroy mutex
  pthread_cond_destroy(&subnode->cond);                             //  Destroy condition variable
  free(subnode);  
    }

    return rv;
}
