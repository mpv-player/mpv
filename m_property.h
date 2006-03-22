
// Get the current value                                                   
#define M_PROPERTY_GET         0                                           
// Get a string representing the current value                             
#define M_PROPERTY_PRINT       1                                           
// Set a new value                                                         
#define M_PROPERTY_SET         2                                           
// Set a new value from a string                                           
#define M_PROPERTY_PARSE       3                                           
// Increment the property                                                  
#define M_PROPERTY_STEP_UP     4                                           
// Decrement the property                                                  
#define M_PROPERTY_STEP_DOWN   5                                           

// Return values for the control function
#define M_PROPERTY_OK                1
#define M_PROPERTY_ERROR             0
// Returned when the property can't be used, for ex something about
// the subs while playing audio only
#define M_PROPERTY_UNAVAILABLE      -1
// Returned if the requested action is not implemented
#define M_PROPERTY_NOT_IMPLEMENTED  -2
// Returned when asking for a property that doesn't exist
#define M_PROPERTY_UNKNOWN          -3
// Returned when the action can't be done (like setting the volume when edl mute)
#define M_PROPERTY_DISABLED         -4

typedef int(*m_property_ctrl_f)(m_option_t* prop,int action,void* arg);

int m_property_do(m_option_t* prop, int action, void* arg);

char* m_property_print(m_option_t* prop);

int m_property_parse(m_option_t* prop, char* txt);

void m_properties_print_help_list(m_option_t* list);

char* m_properties_expand_string(m_option_t* prop_list,char* str);

#define M_PROPERTY_CLAMP(prop,val) do {                                 \
        if(((prop)->flags & M_OPT_MIN) && (val) < (prop)->min)          \
            (val) = (prop)->min;                                        \
        else if(((prop)->flags & M_OPT_MAX) && (val) > (prop)->max)     \
            (val) = (prop)->max;                                        \
    } while(0)

// Implement get
int m_property_int_ro(m_option_t* prop,int action,
                      void* arg,int var);

// Implement set, get and step up/down
int m_property_int_range(m_option_t* prop,int action,
                         void* arg,int* var);

// Same but cycle
int m_property_choice(m_option_t* prop,int action,
                      void* arg,int* var);

// Switch betwen min and max
int m_property_flag(m_option_t* prop,int action,
                    void* arg,int* var);

// Implement get, print
int m_property_float_ro(m_option_t* prop,int action,
                        void* arg,float var);

// Implement set, get and step up/down
int m_property_float_range(m_option_t* prop,int action,
                           void* arg,float* var);

// float with a print function which print the time in ms
int m_property_delay(m_option_t* prop,int action,
                     void* arg,float* var);

// Implement get, print
int m_property_double_ro(m_option_t* prop,int action,
                         void* arg,double var);

// get/print the string
int m_property_string_ro(m_option_t* prop,int action,void* arg, char* str);
