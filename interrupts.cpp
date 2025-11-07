/**
 *
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 *
 */

#include<interrupts.hpp>

void initialize_system() {
    // Initialize 6 fixed partitions
    partition_table = {
        {1, 40, "free"},
        {2, 25, "free"},
        {3, 15, "free"},
        {4, 10, "free"},
        {5, 8, "free"},
        {6, 2, "init"}  // init runs here
    };
    
    // Create init process
    PCB init_process;
    init_process.pid = 0;
    init_process.program_name = "init";
    init_process.partition_number = 6;
    init_process.size = 2;
    init_process.state = "running";
    pcb_table.push_back(init_process);
}


int main(int argc, char** argv) {

    // Partition table (6 partitions)
    std::vector<Partition> partition_table;

    // PCB table
    std::vector<PCB> pcb_table;

    // External files
    std::vector<ExternalFile> external_files;


    //vectors is a C++ std::vector of strings that contain the address of the ISR
    //delays  is a C++ std::vector of ints that contain the delays of each device
    //the index of these elemens is the device number, starting from 0
    auto [vectors, delays] = parse_args(argc, argv);
    std::ifstream input_file(argv[1]);

    std::string trace;      //!< string to store single line of trace file
    std::string execution;  //!< string to accumulate the execution output

    /******************ADD YOUR VARIABLES HERE*************************/

    int current_time = 0;  // keep track of the current time in the simulation

    /******************************************************************/

    //parse each line of the input trace file
    while(std::getline(input_file, trace)) {
        auto [activity, duration_intr] = parse_trace(trace);

        /******************ADD YOUR SIMULATION CODE HERE*************************/
        
        if(activity == "CPU") {
            execution += simulate_cpu(duration_intr, current_time);
        } else if (activity == "SYSCALL" || activity == "END_IO"){
            execution += handle_interrupt(duration_intr, current_time, vectors, delays, activity);
        }

        /************************************************************************/

    }

    input_file.close();

    write_output(execution);

    return 0;
}

/*
* Function to simulate CPU time 
*/

std::string simulate_cpu(int duration, int& current_time) {

    std::string result = std::to_string(current_time) + ", " 
                        + std::to_string(duration) + ", "
                        + "CPU execution\n";
    current_time += duration;
    return result;

}

/*
    ISR execution function, simulates the execution of an ISR for a given device
    device_num: the device number (index in the delays vector)
    current_time: reference to the current time in the simulation
    delays: vector of delays for each device

    returns a string representing the ISR execution log
*/

std::string execute_isr(int device_num, int& current_time, std::vector<int>& delays,
                        const std::string& isr_type) {
    int isr_delay = delays[device_num];
    std::string result = std::to_string(current_time) + ", " 
                        + std::to_string(isr_delay) + ", " 
                        + isr_type + ": run the ISR\n";
    current_time += isr_delay;
    return result;

}

/*
    IRET execution function, simulates the execution of the IRET instruction
    current_time: reference to the current time in the simulation

    returns a string representing the IRET execution log
*/
std::string execute_iret(int& current_time) {
    std::string result = std::to_string(current_time) + ", 1, IRET\n";
    current_time += 1;
    return result;
}

/*
    restore_context function, simulates the restoration of the CPU context
    current_time: reference to the current time in the simulation

    returns a string representing the context restoration log
*/
std::string restore_context(int& current_time) {
    const int CONTEXT_TIME = 10;
    std::string result = std::to_string(current_time) + ", " 
                        + std::to_string(CONTEXT_TIME) + ", " 
                        + "context restored\n";
    current_time += CONTEXT_TIME;
    return result;
}

/*
    switch_to_user_mode function, simulates switching the CPU back to user mode
    current_time: reference to the current time in the simulation

    returns a string representing the switch to user mode log
*/
std::string switch_to_user_mode(int& current_time) {
    std::string result = std::to_string(current_time) + ", 1, switch to user mode\n";
    current_time += 1;
    return result;
}


/*
    handle_interrupt function, simulates the entire interrupt handling process
    device_num: the device number (index in the vectors and delays vectors)
    current_time: reference to the current time in the simulation
    vectors: vector of ISR addresses for each device
    delays: vector of delays for each device
    interrupt_type: string representing the type of interrupt (e.g., "SYSCALL", "END_IO")

    returns a string representing the complete interrupt handling log
*/
std::string handle_interrupt(int device_num, int& current_time, std::vector<std::string>& vectors, std::vector<int>& delays, const std::string& interrupt_type) {

    std::string result = "";
    const int CONTEXT_TIME = 10;

    auto [boilerplate, new_time] = intr_boilerplate(current_time, device_num, CONTEXT_TIME, vectors);

    result += boilerplate;
    current_time = new_time;

    result += execute_isr(device_num, current_time, delays, interrupt_type);
    result += execute_iret(current_time);
    result += restore_context(current_time);
    result += switch_to_user_mode(current_time);

    return result;

}
