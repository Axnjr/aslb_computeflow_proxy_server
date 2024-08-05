#pragma once

#ifndef LB_SCALING_H
#define LB_SCALING_H

#include "aws/ec2/model/TerminateInstancesResponse.h"
#include "aws/ec2/model/TerminateInstancesRequest.h"
#include "aws/ec2/model/DescribeInstancesResponse.h"
#include "aws/ec2/model/DescribeInstancesRequest.h"
#include "aws/ec2/model/RunInstancesRequest.h"
#include "dependencies/httplib.h"
#include "cpr/cpr.h"
#include "aws/core/Aws.h"
#include "aws/ec2/EC2Client.h"
#include "logger.h"

using namespace std;
using namespace logger;

bool scale_up() {

	ltf("scaling.h/scale_up");

	cout << "\n Scaling-up fleet in progress ! \n";

	Aws::SDKOptions options;
	Aws::InitAPI(options);

	try 
	{
		Aws::Client::ClientConfiguration clientConfig;
		Aws::EC2::EC2Client ec2Client(clientConfig);
		Aws::EC2::Model::RunInstancesRequest runRequest;
		Aws::EC2::Model::DescribeInstancesRequest describe;

		runRequest.SetImageId(LB_CONFIG::ami_id);
		runRequest.SetInstanceType(Aws::EC2::Model::InstanceType::t2_micro);
		runRequest.SetMinCount(1);
		runRequest.SetMaxCount(1);

		Aws::EC2::Model::RunInstancesOutcome runOutcome = ec2Client.RunInstances(runRequest);
		if (!runOutcome.IsSuccess()) {

			ltf(
				"Failed to scale VM fleet up based on ami: ",
				LB_CONFIG::ami_id,
				" Error: ",
				runOutcome.GetError().GetMessage(),
				"\n"
			);

			return false;
		}

		const Aws::Vector<Aws::EC2::Model::Instance>& instances = runOutcome.GetResult().GetInstances();
		if (instances.empty()) {

			ltf(
				"Failed to scale VM fleet up based on ami: ",
				LB_CONFIG::ami_id,
				" Error: ",
				runOutcome.GetError().GetMessage(),
				"\n"
			);

			return false;
		}

		string new_vm_ip;
		int max_attempt_to_fetch_ip = 0;
		describe.SetInstanceIds({instances[0].GetInstanceId()});

		while (max_attempt_to_fetch_ip < 10) {

			std::this_thread::sleep_for(std::chrono::seconds(5));
			Aws::EC2::Model::DescribeInstancesOutcome outcome = ec2Client.DescribeInstances(describe);

			if (outcome.IsSuccess()) 
			{
				new_vm_ip = outcome.GetResult().GetReservations()[0].GetInstances()[0].GetPublicIpAddress();

				ltf(
					"ATTEMPTING TO FETCH NEW VM IP ADDRESS! ATTEMPT NO: ",
					max_attempt_to_fetch_ip,
					" Got: ",
					new_vm_ip,
					"\n"
				);

				if (!new_vm_ip.empty()) {
					return true;
				}
			}
			else {
				ltf("Failed to fetch new VM IP address Error: ", outcome.GetError().GetMessage(), "\n");
			}

			max_attempt_to_fetch_ip++;
		}

		LB_CONFIG::IP_POOL.push_back(new_vm_ip);
		LB_CONFIG::vmCount++;
	}
	catch (const std::exception& err) 
	{
		ltf(" ERROR IN SCALING-UP FLEET !! ,", err.what(), "\n");
	}

	Aws::ShutdownAPI(options);
	return false;
}


bool scale_down(string ip) {

	cout << "\n Scaling-down fleet in progress \n";

	Aws::SDKOptions options;
	Aws::InitAPI(options);

	try
	{
		Aws::Client::ClientConfiguration clientConfig;
		Aws::EC2::EC2Client ec2Client(clientConfig);
		Aws::EC2::Model::Filter filter;
		Aws::EC2::Model::DescribeInstancesRequest describe;
		Aws::EC2::Model::TerminateInstancesRequest terminateVm;

		filter.SetName("ip-address");
		filter.AddValues(ip);
		describe.AddFilters(filter);

		auto describeOutcome = ec2Client.DescribeInstances(describe);
		if (!describeOutcome.IsSuccess()) 
		{
			ltf(
				"Failed to Scale-down fleet, Error Fetching VM id: ",
				describeOutcome.GetError().GetMessage(),
				"\n"
			);
			Aws::ShutdownAPI(options);
			return false;
		}

		string vm_to_be_deleted_id = describeOutcome.GetResult().GetReservations()[0].GetInstances()[0].GetInstanceId();
		terminateVm.AddInstanceIds(vm_to_be_deleted_id);

		auto terminateOutcome = ec2Client.TerminateInstances(terminateVm);
		if (!terminateOutcome.IsSuccess()) 
		{
			ltf(
				"Failed to terminate EC2 instance:",
				terminateOutcome.GetError().GetMessage(),
				"\n"
			);
			Aws::ShutdownAPI(options);
			return false;
		}

		ltf("1 VM REMOED FROM FLEET !!");

		Aws::ShutdownAPI(options);
		return true;
	}
	catch (const std::exception& err)
	{
		ltf(" ERROR IN TERMINATING INSTANCE !! ", err.what(), "\n");
	}

	Aws::ShutdownAPI(options);
	return false;
}

#endif // !LB_SCALING_H

// TO BE REPLACED WITH THE ACTUAL DEPLOYMENT ENDPOINT URL 
//cpr::Response r = cpr::Post(cpr::Url{ "http://localhost:3000/api/deploy?withAMI=true" },
//	cpr::Payload{
//		{"amiId", LB_CONFIG::ami_id}
//	}
//);
//std::cout << std::endl << "CPR RESPONSE:-> " << r.status_code << std::endl;
//if (r.status_code == 200) {
//	// notify user about scaling resources
//
//}