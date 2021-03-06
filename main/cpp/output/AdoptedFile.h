#pragma once
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

#include <fstream>
#include <string>
#include <vector>

namespace stride {
namespace output {

class AdoptedFile
{
public:
        /// Constructor
        explicit AdoptedFile(const std::string& output_dir = "output");

        /// Destructor
        ~AdoptedFile();

        /// Print the given cases with corresponding tag.
        void Print(const std::vector<unsigned int>& adopters);

private:
        /// Generate file name and open the file stream.
        void Initialize(const std::string& output_dir);

private:
        std::ofstream m_fstream; ///< The file stream.
};

} // namespace output
} // namespace stride
