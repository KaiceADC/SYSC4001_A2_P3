/**
 *
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 *
 */

#include "Interrupts_101166589_101257741.hpp"
#include <climits>



std::tuple<std::string, std::string, int> simulate_trace(std::vector<std::string> trace_file, int time, std::vector<std::string> vectors, std::vector<int> delays, std::vector<external_file> external_files, PCB current, std::vector<PCB> wait_queue) {

    std::string trace;      //!< string to store single line of trace file
    std::string execution = "";  //!< string to accumulate the execution output
    std::string system_status = "";  //!< string to accumulate the system status output
    int current_time = time;

    //parse each line of the input trace file. 'for' loop to keep track of indices.
    for(size_t i = 0; i < trace_file.size(); i++) {
        auto trace = trace_file[i];

        auto [activity, duration_intr, program_name] = parse_trace(trace);

        if(activity == "CPU") { //As per Assignment 1
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", CPU Burst\n";
            current_time += duration_intr;
        } else if(activity == "SYSCALL") { //As per Assignment 1
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            execution += intr;
            current_time = time;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", SYSCALL ISR (ADD STEPS HERE)\n";
            current_time += delays[duration_intr];

            execution +=  std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } else if(activity == "END_IO") {
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            current_time = time;
            execution += intr;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", ENDIO ISR(ADD STEPS HERE)\n";
            current_time += delays[duration_intr];

            execution +=  std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } else if(activity == "FORK") {
            auto [intr, time] = intr_boilerplate(current_time, 2, 10, vectors);
            execution += intr;
            current_time = time;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //FORK implementation
            
            // Calculate next PID inline
            unsigned int child_pid = 1;
            for (const auto& pcb : wait_queue) {
                if (pcb.PID >= child_pid) child_pid = pcb.PID + 1;
            }
            if (current.PID >= child_pid) child_pid = current.PID + 1;
            
            // Find available partition using BEST FIT algorithm
            int child_partition = -1;
            int best_fit_size = INT_MAX;
            for(int j = 0; j < 6; j++) {
                if(memory[j].code == "empty" && memory[j].size >= current.size) {
                    int wasted_space = memory[j].size - current.size;
                    if(wasted_space < best_fit_size) {
                        best_fit_size = wasted_space;
                        child_partition = memory[j].partition_number;
                    }
                }
            }

            // Declare child PCB outside if block so it's accessible later
            PCB child(child_pid, current.PID, current.program_name, current.size, child_partition);

            if(child_partition == -1) {
                execution += std::to_string(current_time) + ", FORK ERROR: No available partition\n";
            } else {
                execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", cloning the PCB\n";
                memory[child_partition - 1].code = current.program_name;
                current_time += duration_intr;

                execution += std::to_string(current_time) + ", 0, scheduler called\n";
                execution += std::to_string(current_time) + ", 1, IRET\n";
                current_time += 1;

                ///////////////////////////////////////////////////////////////////////////////////////////
                //SYSTEM STATUS for FORK (ADD STEPS HERE)
                
                // Build waiting processes list: parent first, then wait_queue
                std::vector<PCB> fork_waiting_pcbs;
                fork_waiting_pcbs.push_back(current);
                for (const auto& pcb : wait_queue) {
                    fork_waiting_pcbs.push_back(pcb);
                }
                
                // Use helper function to append system status (child is running)
                append_system_status(system_status, current_time, "FORK", duration_intr, 
                                   child, fork_waiting_pcbs);
                
                ///////////////////////////////////////////////////////////////////////////////////////////
            }           
            ///////////////////////////////////////////////////////////////////////////////////////////

            //The following loop helps you do 2 things:
            // * Collect the trace of the child (and only the child, skip parent)
            // * Get the index of where the parent is supposed to start executing from
            std::vector<std::string> child_trace;
            bool skip = true;
            bool exec_flag = false;
            int parent_index = 0;

            for(size_t j = i; j < trace_file.size(); j++) {
                auto [_activity, _duration, _pn] = parse_trace(trace_file[j]);
                if(skip && _activity == "IF_CHILD") {
                    skip = false;
                    continue;
                } else if(_activity == "IF_PARENT"){
                    skip = true;
                    parent_index = j;
                    if(exec_flag) {
                        break;
                    }
                } else if(skip && _activity == "ENDIF") {
                    skip = false;
                    continue;
                } else if(!skip && _activity == "EXEC") {
                    skip = true;
                    child_trace.push_back(trace_file[j]);
                    exec_flag = true;
                }

                if(!skip) {
                    child_trace.push_back(trace_file[j]);
                }
            }
            i = parent_index;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //With the child's trace, run the child (HINT: think recursion)

            if(child_partition != -1 && child_trace.size() > 0) {
                // Create child_wait_queue with parent added
                std::vector<PCB> child_wait_queue = wait_queue;
                child_wait_queue.push_back(current);
                
                auto [child_execution, child_status, new_time] = simulate_trace(
                    child_trace, current_time, vectors, delays, external_files, 
                    child, child_wait_queue);
                execution += child_execution;
                system_status += child_status;
                current_time = new_time;

                memory[child_partition - 1].code = "empty";
            }

            ///////////////////////////////////////////////////////////////////////////////////////////

        } else if(activity == "EXEC") {
            std::cerr << "DEBUG: EXEC activity - program_name = '" << program_name << "'" << std::endl;

            auto [intr, time] = intr_boilerplate(current_time, 3, 10, vectors);
            current_time = time;
            execution += intr;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //EXEC implementation

            // Get program size inline
            unsigned int exec_size = 0;
            for(const auto& file : external_files) {
                if(file.program_name == program_name) {
                    exec_size = file.size;
                    break;
                }
            }

            // Find available partition using BEST FIT algorithm - DECLARE OUTSIDE IF BLOCK
            int avail_exec_partition = -1;
            int best_fit_size = 1000000;
            for(int j = 0; j < 6; j++) {
                if(memory[j].code == "empty" && memory[j].size >= exec_size) {
                    int wasted_space = memory[j].size - exec_size;
                    if(wasted_space < best_fit_size) {
                        best_fit_size = wasted_space;
                        avail_exec_partition = memory[j].partition_number;
                    }
                }
            }

            if (exec_size == 0) {
                execution += std::to_string(current_time) + ", EXEC ERROR: Program not found\n";
            } else if (avail_exec_partition == -1) {
                execution += std::to_string(current_time) + ", EXEC ERROR: No available partition\n";
            } else {
                execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", Program is " 
                                                                + std::to_string(exec_size) + " Mb large\n";
                current_time += duration_intr;

                execution += std::to_string(current_time) + ", " + std::to_string(exec_size * 15) + ", loading program into memory\n";
                current_time += (exec_size * 15);

                execution += std::to_string(current_time) + ", 3, marking partition as occupied\n";
                current_time += 3;

                execution += std::to_string(current_time) + ", 6, updating PCB\n";
                current_time += 6;

                // Free old partition and mark new partition
                memory[current.partition_number - 1].code = "empty";
                memory[avail_exec_partition - 1].code = program_name;

                execution += std::to_string(current_time) + ", 0, scheduler called\n";
                execution += std::to_string(current_time) + ", 1, IRET\n";
                current_time += 1;

                ///////////////////////////////////////////////////////////////////////////////////////////
                
                
                // Create exec'd PCB with new program name and size
                PCB exec_running_pcb(current.PID, current.PPID, program_name, exec_size, avail_exec_partition);
                
                // Use helper function to append system status
                append_system_status(system_status, current_time, "EXEC", duration_intr, 
                                   exec_running_pcb, wait_queue);
                
                ///////////////////////////////////////////////////////////////////////////////////////////

            }

            ///////////////////////////////////////////////////////////////////////////////////////////

            std::ifstream exec_trace_file(program_name + ".txt");

            std::vector<std::string> exec_traces;
            std::string exec_trace;
            while(std::getline(exec_trace_file, exec_trace)) {
                exec_traces.push_back(exec_trace);
            }

            ///////////////////////////////////////////////////////////////////////////////////////////
            //With the exec's trace (i.e. trace of external program), run the exec (HINT: think recursion)

            if(exec_size != 0 && avail_exec_partition != -1) {
                PCB exec_pcb(current.PID, current.PPID, program_name, exec_size, avail_exec_partition);
                
                // Create exec_wait_queue without current process
                std::vector<PCB> exec_wait_queue;
                for (const auto& pcb : wait_queue) {
                    if (pcb.PID != current.PID) {
                        exec_wait_queue.push_back(pcb);
                    }
                }
                
                auto [exec_execution, exec_status, exec_time] = simulate_trace(
                    exec_traces, current_time, vectors, delays, external_files, 
                    exec_pcb, exec_wait_queue);
                execution += exec_execution;
                system_status += exec_status;
                current_time = exec_time;
                
                memory[avail_exec_partition - 1].code = "empty";
            }

            ///////////////////////////////////////////////////////////////////////////////////////////

            break; //Why is this important? (answer in report)

        }
    }

    return {execution, system_status, current_time};
}

int main(int argc, char** argv) {

    //vectors is a C++ std::vector of strings that contain the address of the ISR
    //delays  is a C++ std::vector of ints that contain the delays of each device
    //the index of these elements is the device number, starting from 0
    //external_files is a C++ std::vector of the struct 'external_file'. Check the struct in 
    //interrupt.hpp to know more.
    auto [vectors, delays, external_files] = parse_args(argc, argv);
    std::ifstream input_file(argv[1]);

    //Just a sanity check to know what files you have
    print_external_files(external_files);

    //Make initial PCB (notice how partition is not assigned yet)
    PCB current(0, -1, "init", 1, -1);
    //Update memory (partition is assigned here, you must implement this function)
    if(!allocate_memory(&current)) {
        std::cerr << "ERROR! Memory allocation failed!" << std::endl;
    }

    std::vector<PCB> wait_queue;

    /******************ADD YOUR VARIABLES HERE*************************/
    // All helper logic is now inlined in simulate_trace

    /******************************************************************/

    //Converting the trace file into a vector of strings.
    std::vector<std::string> trace_file;
    std::string trace;
    while(std::getline(input_file, trace)) {
        trace_file.push_back(trace);
    }

    auto [execution, system_status, _] = simulate_trace(trace_file, 
                                            0, 
                                            vectors, 
                                            delays,
                                            external_files, 
                                            current, 
                                            wait_queue);

    input_file.close();

    write_output(execution, "output_files/execution_5.txt");
    write_output(system_status, "output_files/system_status_5.txt");

    return 0;
}