#pragma once
#ifndef TRDROP_TASKS_TASKCONTAINER_H
#define TRDROP_TASKS_TASKCONTAINER_H

#include <functional>
#include <algorithm>
#include <future>

#include <opencv2/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "Tasks.h"
#include "Config.h"

namespace trdrop {

	namespace tasks {

		class TaskScheduler {

			// default member
		public:
			TaskScheduler() = delete;
			TaskScheduler(const TaskScheduler & other) = delete;
			TaskScheduler & operator=(const TaskScheduler & other) = delete;
			TaskScheduler(TaskScheduler && other) = delete;
			TaskScheduler & operator=(TaskScheduler && other) = delete;
			~TaskScheduler() = default;

			// specialized member
		public:
			TaskScheduler(std::vector<cv::VideoCapture> & inputs)
				: inputs(inputs)
				, prev(inputs.size())
				, cur(inputs.size())
			{ }

			void addPreTask(trdrop::tasks::pretask task) {
				preTasks.push_back(task);
			}

			void addInterTask(trdrop::tasks::intertask task) {
				interTasks.push_back(task);
			}

			void addPostTask(trdrop::tasks::posttask task) {
				postTasks.push_back(task);
			}
			
			void merge(std::vector<cv::Mat> & frames, cv::Mat & result) {

				int x = frames[0].size().width / 4; // TODO only works for max 2 videos
				int y = 0;
				int width = frames[0].size().width / 2; // static_cast<int>(frames.size());
				int height = frames[0].size().height;

				cv::Rect box(x, y, width, height);
				cv::Rect left(0, 0, width, height);
				cv::Rect right(x*2, 0, width, height);

				if (frames.size() == 1) {
					result = cv::Mat(frames[0].size(), true);
					frames[0].copyTo(result);
				} else if (frames.size() == 2) {
					frames[0].copyTo(result);
					cv::Mat cropped0 = frames[0](box);
					cv::Mat cropped1 = frames[1](box);
					cropped0.copyTo(result(left));
					cropped1.copyTo(result(right));
				} 
			}

			bool next() {
#if _DEBUG
				std::cout << "\nDEBUG: TaskScheduler.next() - currentFrameIndex\n";
#endif
				if (currentFrameIndex == 0) {
					trdrop::util::enumerate(inputs.begin(), inputs.end(), 0, [&](unsigned vix, cv::VideoCapture input) {
						readSuccessful &= input.read(prev[vix]);
						readSuccessful &= input.read(cur[vix]);
					});

					if (readSuccessful) {
						merged = cv::Mat(prev[0].size().height, prev[0].size().width, prev[0].depth());
					}
				} else {
					trdrop::util::enumerate(inputs.begin(), inputs.end(), 0, [&](unsigned vix, cv::VideoCapture input) {
						cur[vix].copyTo(prev[vix]);  // TODO swap pointers
						readSuccessful &= input.read(cur[vix]);
					});
				}

				if (readSuccessful) {
					// if this is the first frame, we load two frames
					currentFrameIndex += currentFrameIndex == 0 ? 2 : 1;
#if _DEBUG
					std::cout << "DEBUG: TaskScheduler.readSuccesful, currentFrameIndex: " << currentFrameIndex << '\n';
#endif
					
					// pretasks - parallel
					trdrop::util::enumerate(inputs.begin(), inputs.end(), 0, [&](unsigned vix, cv::VideoCapture input) {
						std::for_each(preTasks.begin(), preTasks.end(), [&](trdrop::tasks::pretask f) {
							preTasksFinished.push_back(std::move(std::async(std::launch::async, f, prev[vix], cur[vix], currentFrameIndex, vix)));
						});
					}); 

#if _DEBUG
					std::cout << "DEBUG: TaskScheduler - launched " << preTasksFinished.size() << " pretasks\n";
#endif				
					// pretasks - waiting
					std::for_each(preTasksFinished.begin(), preTasksFinished.end(), [](std::future<void> & future){
						future.wait();
					}); 
					preTasksFinished.clear();
					
#if _DEBUG
					std::cout << "DEBUG: TaskScheduler - finished all pretasks - size: " << preTasksFinished.size() << "\n";
#endif
					// intermediate tasks - parallel of different videos
					trdrop::util::enumerate(inputs.begin(), inputs.end(), 0, [&](unsigned vix, cv::VideoCapture input) {
						/*
						std::function<void()> videoTask = [&]() {
							std::for_each(interTasks.begin(), interTasks.end(), [&](trdrop::tasks::intertask f) {
								// interTasksFinished.push_back(std::move(std::async(std::launch::async, f, prev[vix], vix)));
								f(prev[vix], currentFrameIndex, vix);
							});
						};
						
						interTasksFinished.push_back(std::move(std::async(std::launch::async, videoTask)));
						*/
						std::for_each(interTasks.begin(), interTasks.end(), [&](trdrop::tasks::intertask f) {
							interTasksFinished.push_back(std::move(std::async(std::launch::async, f, prev[vix], currentFrameIndex, vix)));
							//f(prev[vix], currentFrameIndex, vix);
						}); 
					});
#if _DEBUG
					std::cout << "DEBUG: TaskScheduler - launched " << interTasksFinished.size() * interTasks.size() << " intermediate tasks\n";
#endif
					// intermediate tasks - waiting
					std::for_each(interTasksFinished.begin(), interTasksFinished.end(), [](std::future<void> & future) {
						future.wait();
					});
					interTasksFinished.clear();

#if _DEBUG
					std::cout << "DEBUG: TaskScheduler - finished all intermediate tasks\n";
#endif
					// merging
					merge(prev, merged);
#if _DEBUG
					std::cout << "DEBUG: TaskScheduler - merged frames\n";
#endif
					// post tasks - sequential
					std::for_each(postTasks.begin(), postTasks.end(), [&](trdrop::tasks::posttask f) { f(merged); });
#if _DEBUG
					std::cout << "DEBUG: TaskScheduler - finished all posttasks\n";
#endif
				}
				
				return readSuccessful;
			}

			size_t getCurrentFrameIndex() {
				return currentFrameIndex;
			}

			// private member
		private:
			std::vector<cv::VideoCapture> & inputs;
			bool readSuccessful = true;

			std::vector<trdrop::tasks::pretask> preTasks;
			std::vector<std::future<void>>		preTasksFinished;
			
			std::vector<trdrop::tasks::intertask> interTasks;
			std::vector<std::future<void>>		  interTasksFinished;
			
			std::vector<trdrop::tasks::posttask> postTasks;
			
			std::vector<cv::Mat> cur;
			std::vector<cv::Mat> prev;
			cv::Mat				 merged;
			size_t currentFrameIndex = 0;
		};

	} // namespace tasks
} // namespace trdrop

#endif // !TRDROP_TASKS_TASKCONTAINER_H