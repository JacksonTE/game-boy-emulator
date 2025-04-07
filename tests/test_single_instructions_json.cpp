#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <vector>
#include "cpu.h"
#include "machine_cycle_interaction.h"
#include "memory_management_unit.h"
#include "register_file.h"

namespace {

struct AddressValuePair {
    uint16_t memory_address;
    uint8_t value;

    AddressValuePair(uint16_t address, uint8_t value)
        : memory_address{address},
          value{value} {
    }
};

struct SingleInstructionTestCase {
    std::string test_name;

    GameBoy::RegisterFile<std::endian::native> initial_register_values;
    std::vector<AddressValuePair> initial_ram_values;

    GameBoy::RegisterFile<std::endian::native> expected_register_values;
    std::vector<AddressValuePair> expected_ram_values;

    std::vector<GameBoy::MachineCycleInteraction> expected_memory_interactions;
};

static std::vector<std::filesystem::path> get_ordered_json_test_file_paths(const std::filesystem::path &directory) {
    std::vector<std::filesystem::path> filenames;

    for (const auto &entry : std::filesystem::directory_iterator(directory)) {
        // Skip Halt and Stop tests - they aren't relevant
        if (entry.is_regular_file() && entry.path().extension() == ".json" &&
            entry.path().filename() != "10.json" && entry.path().filename() != "76.json") {
            filenames.push_back(entry.path());
        }
    }
    std::sort(filenames.begin(), filenames.end(), [](const std::filesystem::path &a, const std::filesystem::path &b) {
        std::string a_string = a.string();
        std::string b_string = b.string();
        if (a_string.size() != b_string.size()) {
            return a_string.size() < b_string.size();
        }
        return a_string < b_string;
    });

    return filenames;
}

static std::vector<SingleInstructionTestCase> load_test_cases_from_json_file(const std::filesystem::path &json_test_file_path) {
    std::ifstream file(json_test_file_path);
    if (!file) {
        throw std::runtime_error("Error: could not open JSON file " + json_test_file_path.string());
    }

    nlohmann::json json_test_file_object;
    file >> json_test_file_object;
    std::vector<SingleInstructionTestCase> json_test_cases;

    for (const auto &test_case_object : json_test_file_object) {
        SingleInstructionTestCase test_case;

        test_case.test_name = test_case_object["name"].get<std::string>();

        for (const auto &initial_ram_array : test_case_object["initial"]["ram"]) {
            const auto &address = initial_ram_array[0];
            const auto &initial_value = initial_ram_array[1];
            test_case.initial_ram_values.emplace_back(address, initial_value);
        }
        test_case.initial_register_values.program_counter = test_case_object["initial"]["pc"].get<uint16_t>();
        test_case.initial_register_values.stack_pointer = test_case_object["initial"]["sp"].get<uint16_t>();
        test_case.initial_register_values.a = test_case_object["initial"]["a"].get<uint8_t>();
        test_case.initial_register_values.b = test_case_object["initial"]["b"].get<uint8_t>();
        test_case.initial_register_values.c = test_case_object["initial"]["c"].get<uint8_t>();
        test_case.initial_register_values.d = test_case_object["initial"]["d"].get<uint8_t>();
        test_case.initial_register_values.e = test_case_object["initial"]["e"].get<uint8_t>();
        test_case.initial_register_values.flags = test_case_object["initial"]["f"].get<uint8_t>();
        test_case.initial_register_values.h = test_case_object["initial"]["h"].get<uint8_t>();
        test_case.initial_register_values.l = test_case_object["initial"]["l"].get<uint8_t>();

        for (const auto &expected_ram_array : test_case_object["final"]["ram"]) {
            const auto &address = expected_ram_array[0];
            const auto &final_value = expected_ram_array[1];
            test_case.expected_ram_values.emplace_back(address, final_value);
        }
        test_case.expected_register_values.program_counter = test_case_object["final"]["pc"].get<uint16_t>();
        test_case.expected_register_values.stack_pointer = test_case_object["final"]["sp"].get<uint16_t>();
        test_case.expected_register_values.a = test_case_object["final"]["a"].get<uint8_t>();
        test_case.expected_register_values.b = test_case_object["final"]["b"].get<uint8_t>();
        test_case.expected_register_values.c = test_case_object["final"]["c"].get<uint8_t>();
        test_case.expected_register_values.d = test_case_object["final"]["d"].get<uint8_t>();
        test_case.expected_register_values.e = test_case_object["final"]["e"].get<uint8_t>();
        test_case.expected_register_values.flags = test_case_object["final"]["f"].get<uint8_t>();
        test_case.expected_register_values.h = test_case_object["final"]["h"].get<uint8_t>();
        test_case.expected_register_values.l = test_case_object["final"]["l"].get<uint8_t>();

        for (const auto &cycles_array : test_case_object["cycles"]) {
            const auto &address_accessed = cycles_array[0];
            const auto &value_written = cycles_array[1];
            const auto &operation_performed = cycles_array[2];

            if (operation_performed == "---") {
                test_case.expected_memory_interactions.emplace_back(GameBoy::MemoryOperation::None);
            }
            else if (operation_performed == "r-m") {
                test_case.expected_memory_interactions.emplace_back(GameBoy::MemoryOperation::Read, address_accessed);
            }
            else {
                test_case.expected_memory_interactions.emplace_back(GameBoy::MemoryOperation::Write, address_accessed, value_written);
            }
        }

        json_test_cases.push_back(test_case);
    }
    return json_test_cases;
}

// The single instruction tests expect the memory to be a 64KB flat array with no internal read/write restrictions
class SingleInstructionTestMemory : public GameBoy::MemoryManagementUnit {
public:
    SingleInstructionTestMemory() {
        flat_memory = std::make_unique<uint8_t[]>(GameBoy::MEMORY_SIZE);
        std::fill_n(flat_memory.get(), GameBoy::MEMORY_SIZE, 0);
    }

    void reset_state() override {
        std::fill_n(flat_memory.get(), GameBoy::MEMORY_SIZE, 0);
    }

    uint8_t read_byte(uint16_t address) const override {
        return flat_memory[address];
    }

    void write_byte(uint16_t address, uint8_t value) override {
        flat_memory[address] = value;
    }

private:
    std::unique_ptr<uint8_t[]> flat_memory;
};

class SingleInstructionTest : public testing::TestWithParam<std::filesystem::path> {
protected:
    std::vector<GameBoy::MachineCycleInteraction> memory_interactions;
    std::unique_ptr<SingleInstructionTestMemory> memory_interface;
    GameBoy::CPU game_boy_cpu;

    SingleInstructionTest()
        : memory_interface{std::make_unique<SingleInstructionTestMemory>()},
          game_boy_cpu{*memory_interface,
                       [this](GameBoy::MachineCycleInteraction interaction) {
                           this->memory_interactions.push_back(interaction);
                       }} {
    }

    void set_initial_values(const SingleInstructionTestCase &test_case) {
        memory_interactions.clear();
        memory_interface->reset_state();
        game_boy_cpu.reset_state();

        for (const AddressValuePair &pair : test_case.initial_ram_values) {
            memory_interface->write_byte(pair.memory_address, pair.value);
        }
        game_boy_cpu.set_register_file_state(test_case.initial_register_values);
    }
};

} // namespace

TEST_P(SingleInstructionTest, JsonTestCasesFile) {
    std::filesystem::path json_test_filename = GetParam();
    SCOPED_TRACE("Test file: " + json_test_filename.string());

    auto test_cases = load_test_cases_from_json_file(json_test_filename);

    for (int i = 0; i < test_cases.size(); i++) {
        const auto &test_case = test_cases.at(i);
        SCOPED_TRACE("Test name: " + test_case.test_name);

        set_initial_values(test_case);
        game_boy_cpu.step(); // Execute initial NOP (no operation) and fetch first instruction
        game_boy_cpu.step();

        EXPECT_EQ(game_boy_cpu.get_register_file().af, test_case.expected_register_values.af);
        EXPECT_EQ(game_boy_cpu.get_register_file().bc, test_case.expected_register_values.bc);
        EXPECT_EQ(game_boy_cpu.get_register_file().de, test_case.expected_register_values.de);
        EXPECT_EQ(game_boy_cpu.get_register_file().hl, test_case.expected_register_values.hl);
        EXPECT_EQ(static_cast<uint16_t>(game_boy_cpu.get_register_file().program_counter - 1), test_case.expected_register_values.program_counter); // Compare expected program_counter against program_counter-1 since next instruction is fetched at the end of the current one
        EXPECT_EQ(game_boy_cpu.get_register_file().stack_pointer, test_case.expected_register_values.stack_pointer);

        for (const AddressValuePair &expected_pair : test_case.expected_ram_values) {
            EXPECT_EQ(memory_interface->read_byte(expected_pair.memory_address), expected_pair.value);
        }

        EXPECT_EQ(memory_interactions.size() - 1, test_case.expected_memory_interactions.size());
        for (int i = 0; i < test_case.expected_memory_interactions.size(); i++) {
            EXPECT_EQ(memory_interactions.at(i), test_case.expected_memory_interactions.at(i));
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    SingleInstructionTestSuite,
    SingleInstructionTest,
    testing::ValuesIn(
        get_ordered_json_test_file_paths(
            std::filesystem::path(PROJECT_ROOT) / "tests" / "data" / "single-instructions-json" / "sm83" / "v1"
        )
    )
);

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
