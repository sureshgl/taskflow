// This program demonstrates how to create a pipeline scheduling framework
// that propagates a series of integers and adds one to the result at each
// stage, using a range of pipes provided by the application.
//
// The pipeline has the following structure:
//
// o -> o -> o
// |    |    |
// v    v    v
// o -> o -> o
// |    |    |
// v    v    v
// o -> o -> o
// |    |    |
// v    v    v
// o -> o -> o
//
// Then, the program resets the pipeline to a new range of five pipes.
//
// o -> o -> o -> o -> o
// |    |    |    |    |
// v    v    v    v    v
// o -> o -> o -> o -> o
// |    |    |    |    |
// v    v    v    v    v
// o -> o -> o -> o -> o
// |    |    |    |    |
// v    v    v    v    v
// o -> o -> o -> o -> o

#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/pipeline.hpp>

int main() {

  tf::Taskflow taskflow("pipeline");
  
  tf::Executor executor;

  tf::Taskflow sub_taskflow("sub_taskflow");
  auto task1 = sub_taskflow.emplace([](tf::WorkerView wv, tf::TaskView tv, tf::Pipeflow& pf){ std::cout << "F1 TaskA\n"; });
  auto task2 = sub_taskflow.emplace([](tf::WorkerView wv, tf::TaskView tv, tf::Pipeflow& pf){ std::cout << "F1 TaskB\n"; });
  auto task3 = sub_taskflow.emplace([](tf::WorkerView wv, tf::TaskView tv, tf::Pipeflow& pf){ std::cout << "F1 TaskC\n"; });
  task1.name("task1");
  task2.name("task2");
  task3.name("task3");
  task1.precede(task2);
  task2.precede(task3);



  const size_t num_lines = 4;

  // create data storage
  std::array<int, num_lines> buffer;
  
  // define the pipe callable
  auto pipe_callable = [&buffer, &sub_taskflow, &executor] (tf::Pipeflow& pf) mutable {
    switch(pf.pipe()) {
      // first stage generates only 5 scheduling tokens and saves the 
      // token number into the buffer.
      case 0: {
        if(pf.token() == 5) {
          pf.stop();
        }
        else {
          printf("stage 1: input token = %zu\n", pf.token());
          //buffer[pf.line()] = pf.token();
          tf::Taskflow *myTaskflow = sub_taskflow.clone();
          executor.run(*myTaskflow).wait();
        }
        return;
      }
      break;
      
      // other stages propagate the previous result to this pipe and
      // increment it by one
      default: {
        printf(
          "stage %zu: input buffer[%zu] = %d\n", pf.pipe(), pf.line(), buffer[pf.line()]
        );
        buffer[pf.line()] = buffer[pf.line()] + 1;
      } 
      break;
    }
  };

  // create a vector of three pipes
  std::vector< tf::Pipe<std::function<void(tf::Pipeflow&)>> > pipes;

  for(size_t i=0; i<3; i++) {
    pipes.emplace_back(tf::PipeType::SERIAL, pipe_callable);
  }
  
  // create a pipeline of four parallel lines using the given vector of pipes
  tf::ScalablePipeline pl(num_lines, pipes.begin(), pipes.end());

  // build the pipeline graph using composition
  tf::Task init = taskflow.emplace([](tf::WorkerView wv, tf::TaskView tv, tf::Pipeflow& pf){ std::cout << "ready\n"; })
                          .name("starting pipeline");
  tf::Task task = taskflow.composed_of(pl)
                          .name("pipeline");
  tf::Task stop = taskflow.emplace([](tf::WorkerView wv, tf::TaskView tv, tf::Pipeflow& pf){ std::cout << "stopped\n"; })
                          .name("pipeline stopped");

  // create task dependency
  init.precede(task);
  task.precede(stop);
  
  // dump the pipeline graph structure (with composition)
  taskflow.dump(std::cout);

  // run the pipeline
  executor.run(taskflow).wait();

  // reset the pipeline to a new range of five pipes and starts from
  // the initial state (i.e., token counts from zero)
  for(size_t i=0; i<2; i++) {
    pipes.emplace_back(tf::PipeType::SERIAL, pipe_callable);
  }
  pl.reset(pipes.begin(), pipes.end());

  executor.run(taskflow).wait();

  return 0;
}



