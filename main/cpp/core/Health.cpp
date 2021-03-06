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

#include "Health.h"

#include <cassert>

namespace stride {

Health::Health(unsigned int start_infectiousness, unsigned int start_symptomatic, unsigned int time_infectious,
               unsigned int time_symptomatic)
    : m_disease_counter(0U), m_status(HealthStatus::Susceptible), m_start_infectiousness(start_infectiousness),
      m_start_symptomatic(start_symptomatic)
{
        m_end_infectiousness = start_infectiousness + time_infectious;
        m_end_symptomatic    = start_symptomatic + time_symptomatic;
}

void Health::SetImmune() { m_status = HealthStatus::Immune; }

void Health::SetSusceptible() { m_status = HealthStatus::Susceptible; }

void Health::StartInfection()
{
        assert(m_status == HealthStatus::Susceptible && "Health::StartInfection: m_health_status == "
                                                        "DiseaseStatus::Susceptible fails.");
        m_status = HealthStatus::Exposed;
        ResetDiseaseCounter();
}

void Health::StopInfection()
{
        assert((m_status == HealthStatus::Exposed || m_status == HealthStatus::Infectious ||
                m_status == HealthStatus::Symptomatic || m_status == HealthStatus::InfectiousAndSymptomatic) &&
               "Health::StopInfection> person not infected");
        m_status = HealthStatus::Recovered;
}

void Health::Update()
{
        const bool infected = m_status == HealthStatus::Exposed || m_status == HealthStatus::Infectious ||
                              m_status == HealthStatus::Symptomatic ||
                              m_status == HealthStatus::InfectiousAndSymptomatic;

        if (infected) {
                IncrementDiseaseCounter();
                if (GetDiseaseCounter() == m_start_infectiousness) {
                        if (m_status == HealthStatus::Symptomatic) {
                                m_status = HealthStatus::InfectiousAndSymptomatic;
                        } else {
                                m_status = HealthStatus::Infectious;
                        }
                }
                if (GetDiseaseCounter() == m_start_symptomatic) {
                        if (m_status == HealthStatus::Infectious) {
                                m_status = HealthStatus::InfectiousAndSymptomatic;
                        } else {
                                m_status = HealthStatus::Symptomatic;
                        }
                }
                if (GetDiseaseCounter() == m_end_symptomatic) {
                        if (m_status == HealthStatus::InfectiousAndSymptomatic) {
                                m_status = HealthStatus::Infectious;
                        } else {
                                StopInfection();
                        }
                }
                if (GetDiseaseCounter() == m_end_infectiousness) {
                        if (m_status == HealthStatus::InfectiousAndSymptomatic) {
                                m_status = HealthStatus::Symptomatic;
                        } else {
                                StopInfection();
                        }
                }
        }
}

} /* namespace stride */
