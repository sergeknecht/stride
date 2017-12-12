/*
 *  This is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  any later version.
 *  The software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  You should have received a copy of the GNU General Public License
 *  along with the software. If not, see <http://www.gnu.org/licenses/>.
 *
 *  Copyright 2017, Kuylen E, Willem L, Broeckhove J
 */

/**
 * @file
 * Implementation for StrideRunner.
 */

#include "StrideRunner.h"

#include "output/AdoptedFile.h"
#include "output/CasesFile.h"
#include "output/PersonFile.h"
#include "output/SummaryFile.h"
#include "sim/SimulatorBuilder.h"
#include "util/ConfigInfo.h"
#include "util/InstallDirs.h"
#include "util/StringUtils.h"
#include "util/TimeStamp.h"
#include "util/TimeToString.h"

#include <boost/filesystem.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <iostream>
#include <omp.h>
#include <spdlog/spdlog.h>

namespace stride {

using namespace output;
using namespace util;
using namespace boost::filesystem;
using namespace boost::property_tree;
using namespace std;
using namespace std::chrono;

///
bool StrideRunner::m_is_running = false;
///
bool StrideRunner::m_operational = false;
///
std::string StrideRunner::m_output_prefix = "";
///
boost::property_tree::ptree StrideRunner::m_pt_config = boost::property_tree::ptree();
///
Stopwatch<> StrideRunner::m_total_clock = Stopwatch<>("total_clock");
///
shared_ptr<Simulator> StrideRunner::m_sim = make_shared<Simulator>();

/// Register observer
void StrideRunner::RegisterObserver(std::shared_ptr<SimulatorObserver>& observer) { m_sim->Register(observer); }

///
void StrideRunner::Setup(bool track_index_case, const std::string& config_file_name, bool use_install_dirs)
{
	// -----------------------------------------------------------------------------------------
	// Print output to command line.
	// -----------------------------------------------------------------------------------------
	cout << "\n*************************************************************" << endl;
	cout << "Starting up at:      " << TimeStamp().ToString() << endl;

	if (use_install_dirs) {
		cout << "Executing:           " << InstallDirs::GetExecPath().string() << endl;
		cout << "Current directory:   " << InstallDirs::GetCurrentDir().string() << endl;
		cout << "Install directory:   " << InstallDirs::GetRootDir().string() << endl;
		cout << "Data    directory:   " << InstallDirs::GetDataDir().string() << endl;

		// -----------------------------------------------------------------------------------------
		// Check execution environment.
		// -----------------------------------------------------------------------------------------
		if (InstallDirs::GetCurrentDir().compare(InstallDirs::GetRootDir()) != 0) {
			throw runtime_error(string(__func__) + "> Current directory is not install root! Aborting.");
		}
		if (InstallDirs::GetDataDir().empty()) {
			throw runtime_error(string(__func__) + "> Data directory not present! Aborting.");
		}
	}

	// -----------------------------------------------------------------------------------------
	// Configuration.
	// -----------------------------------------------------------------------------------------
	const auto file_path = canonical(system_complete(config_file_name));

	if (!is_regular_file(file_path)) {
		throw runtime_error(string(__func__) + ">Config file " + file_path.string() +
				    " not present. Aborting.");
	}

	read_xml(file_path.string(), m_pt_config);
	cout << "Configuration file:  " << file_path.string() << endl;

	// -----------------------------------------------------------------------------------------
	// OpenMP.
	// -----------------------------------------------------------------------------------------
	unsigned int num_threads;

#pragma omp parallel
	{
		num_threads = omp_get_num_threads();
	}
	if (ConfigInfo::HaveOpenMP()) {
		cout << "Using OpenMP threads:  " << num_threads << endl;
	} else {
		cout << "Not using OpenMP threads." << endl;
	}

	// -----------------------------------------------------------------------------------------
	// Set output path prefix.
	// -----------------------------------------------------------------------------------------
	m_output_prefix = m_pt_config.get<string>("run.output_prefix", "");
	if (m_output_prefix.length() == 0) {
		m_output_prefix = TimeStamp().ToTag();
	}
	cout << "Project output tag:  " << m_output_prefix << endl << endl;

	// -----------------------------------------------------------------------------------------
	// Additional run configurations.
	// -----------------------------------------------------------------------------------------
	if (m_pt_config.get_optional<bool>("run.num_participants_survey") == false) {
		m_pt_config.put("run.num_participants_survey", 1);
	}

	// -----------------------------------------------------------------------------------------
	// Track index case setting.
	// -----------------------------------------------------------------------------------------
	cout << "Setting for track_index_case:  " << boolalpha << track_index_case << endl;

	// -----------------------------------------------------------------------------------------
	// Create logger
	// Transmissions:     [TRANSMISSION] <infecterID> <infectedID> <clusterID> <day>
	// General contacts:  [CNT] <person1ID> <person1AGE> <person2AGE>  <at_home> <at_work> <at_school> <at_other>
	// -----------------------------------------------------------------------------------------
	spdlog::set_async_mode(1048576);
	boost::filesystem::path logfile_path = m_output_prefix;
	logfile_path /= "logfile";
	auto file_logger =
	    spdlog::rotating_logger_mt("contact_logger", logfile_path.c_str(), std::numeric_limits<size_t>::max(),
				       std::numeric_limits<size_t>::max());
	file_logger->set_pattern("%v"); // Remove meta data from log => time-stamp of logging

	// ------------------------------------------------------------------------------
	// Create the simulator.
	//------------------------------------------------------------------------------
	m_total_clock.Start();
	cout << "Building the simulator. " << endl;
	m_sim = SimulatorBuilder::Build(m_pt_config, num_threads, track_index_case);
	cout << "Done building the simulator. " << endl;

	// -----------------------------------------------------------------------------------------
	// Check the simulator.
	// -----------------------------------------------------------------------------------------
	m_operational = m_sim->IsOperational();
	if (m_operational) {
		cout << "Done checking the simulator. " << endl << endl;
	} else {
		file_logger->info("[ERROR] Invalid configuration");
		cout << "Invalid configuration => terminate without output" << endl << endl;
	}
}

/// Run the simulator with config information provided.
void StrideRunner::Run()
{
	// -----------------------------------------------------------------------------------------
	// Run the simulation (if operational).
	// -----------------------------------------------------------------------------------------
	Stopwatch<> run_clock("run_clock");
	if (m_operational) {
		m_is_running = true;
		const unsigned int num_days = m_pt_config.get<unsigned int>("run.num_days");
		vector<unsigned int> cases(num_days);
		vector<unsigned int> adopted(num_days);
		for (unsigned int i = 0; i < num_days; i++) {
			// Check if still running
			if (!m_is_running) {
				break;
			}

			cout << "Simulating day: " << setw(5) << i;
			run_clock.Start();
			m_sim->TimeStep();
			run_clock.Stop();

			cout << "     Done, infected count: ";

			cases[i] = m_sim->GetPopulation()->GetInfectedCount();
			adopted[i] = m_sim->GetPopulation()->GetAdoptedCount();

			cout << setw(7) << cases[i] << "     Adopters count: " << setw(7) << adopted[i];
			cout << endl;
		}

		// -----------------------------------------------------------------------------------------
		// Generate output files
		// -----------------------------------------------------------------------------------------
		GenerateOutputFiles(m_output_prefix, cases, adopted, m_pt_config,
				    duration_cast<milliseconds>(run_clock.Get()).count(),
				    duration_cast<milliseconds>(m_total_clock.Get()).count());

		m_is_running = false;
	}

	// Clean up
	spdlog::drop_all();

	// -----------------------------------------------------------------------------------------
	// Print final message to command line.
	// -----------------------------------------------------------------------------------------
	cout << endl << endl;
	cout << "  run_time: " << run_clock.ToString() << "  -- total time: " << m_total_clock.ToString() << endl
	     << endl;
	cout << "Exiting at:         " << TimeStamp().ToString() << endl << endl;
}

void StrideRunner::Stop() { m_is_running = false; }

/// Generate output files (at the end of the simulation).
void StrideRunner::GenerateOutputFiles(const std::string& output_prefix, const std::vector<unsigned int>& cases,
				       const std::vector<unsigned int>& adopted,
				       const boost::property_tree::ptree& pt_config, const unsigned int run_time,
				       const unsigned int total_time)
{
	// Cases
	CasesFile cases_file(output_prefix);
	cases_file.Print(cases);

	// Adopted
	AdoptedFile adopted_file(output_prefix);
	adopted_file.Print(adopted);

	// Summary
	SummaryFile summary_file(output_prefix);
	summary_file.Print(pt_config, m_sim->GetPopulation()->size(), m_sim->GetPopulation()->GetInfectedCount(),
			   m_sim->GetDiseaseProfile().GetTransmissionRate(), run_time, total_time);

	// Persons
	if (pt_config.get<double>("run.generate_person_file") == 1) {
		PersonFile person_file(output_prefix);
		person_file.Print(m_sim->GetPopulation());
	}
}

} // end_of_namespace