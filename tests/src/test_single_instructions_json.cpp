#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <vector>

#include "central_processing_unit.h"
#include "memory_management_unit.h"
#include "pixel_processing_unit.h"
#include "register_file.h"

enum class MemoryInteraction
{
    None,
    Read,
    Write
};

struct MachineCycleOperation
{
    MemoryInteraction memory_interaction;
    uint16_t address_accessed{};
    uint8_t value_written{};

    MachineCycleOperation(MemoryInteraction interaction)
        : memory_interaction{interaction}
    {
    }

    MachineCycleOperation(MemoryInteraction interaction, uint16_t address)
        : memory_interaction{interaction},
          address_accessed{address}
    {
    }

    MachineCycleOperation(MemoryInteraction interaction, uint16_t address, uint8_t value)
        : memory_interaction{interaction},
          address_accessed{address},
          value_written{value}
    {
    }

    bool operator==(const MachineCycleOperation &other) const
    {
        if (memory_interaction != other.memory_interaction)
            return false;

        switch (memory_interaction)
        {
            case MemoryInteraction::None:
                return true;
            case MemoryInteraction::Read:
                return address_accessed == other.address_accessed;
            case MemoryInteraction::Write:
                return address_accessed == other.address_accessed && value_written == other.value_written;
            default:
                return false;
        }
    }

};

static std::string memory_interaction_to_string(MemoryInteraction operation)
{
    switch (operation) {
        case MemoryInteraction::None:
            return "None";
        case MemoryInteraction::Read:
            return "Read";
        case MemoryInteraction::Write:
            return "Write";
        default:
            return "Unknown";
    }
}

static std::ostream &operator<<(std::ostream &output_stream, const MachineCycleOperation &interaction)
{
    output_stream << "MachineCycleOperation{"
                  << "memory_interaction: " << memory_interaction_to_string(interaction.memory_interaction) << ", "
                  << "address_accessed: " << (interaction.address_accessed ? std::format("{:04x}", interaction.address_accessed) : "none") << ", "
                  << "value_written: " << (interaction.value_written ? std::format("{:04x}", interaction.value_written) : "none")
                  << "}";
    return output_stream;
}

struct SingleInstructionTestCase
{
    std::string test_name;

    GameBoyCore::RegisterFile<std::endian::native> initial_register_values;
    std::vector<std::pair<uint16_t, uint8_t>> initial_ram_address_value_pairs;

    GameBoyCore::RegisterFile<std::endian::native> expected_register_values;
    std::vector<std::pair<uint16_t, uint8_t>> expected_ram_address_value_pairs;

    std::vector<MachineCycleOperation> expected_memory_interactions;
};

static std::vector<std::filesystem::path> get_ordered_json_test_file_paths()
{
    const std::filesystem::path directory = std::filesystem::path(PROJECT_ROOT) / "tests" / "data" / "single-instructions-json" / "sm83" / "v1";
    if (!std::filesystem::exists(directory))
        return { directory };

    std::vector<std::filesystem::path> json_test_file_paths;

    for (const auto &entry : std::filesystem::directory_iterator(directory))
    {
        // Skip 'halt' and 'stop' tests - their correct functionality isn't tested well with the json tests
        if (entry.is_regular_file() && entry.path().extension() == ".json" &&
            entry.path().filename() != "10.json" && entry.path().filename() != "76.json")
        {
            json_test_file_paths.push_back(entry.path());
        }
    }
    std::sort(json_test_file_paths.begin(), json_test_file_paths.end(), [](const std::filesystem::path &a, const std::filesystem::path &b)
    {
        std::string a_string = a.string();
        std::string b_string = b.string();

        return a_string.size() == b_string.size()
            ? a_string < b_string
            : a_string.size() < b_string.size();
    });
    return json_test_file_paths;
}

static std::vector<SingleInstructionTestCase> load_test_cases_from_json_file(const std::filesystem::path &json_test_file_path)
{
    std::ifstream file(json_test_file_path);
    if (!file)
        throw std::runtime_error("Error: could not open JSON file " + json_test_file_path.string());

    nlohmann::json json_test_file_object;
    file >> json_test_file_object;
    std::vector<SingleInstructionTestCase> json_test_cases;

    for (const auto &test_case_object : json_test_file_object)
    {
        SingleInstructionTestCase test_case;

        test_case.test_name = test_case_object["name"].get<std::string>();

        for (const auto &initial_ram_array : test_case_object["initial"]["ram"])
        {
            const auto &address = initial_ram_array[0];
            const auto &initial_value = initial_ram_array[1];
            test_case.initial_ram_address_value_pairs.emplace_back(address, initial_value);
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

        for (const auto &expected_ram_array : test_case_object["final"]["ram"])
        {
            const auto &address = expected_ram_array[0];
            const auto &expected_final_value = expected_ram_array[1];
            test_case.expected_ram_address_value_pairs.emplace_back(address, expected_final_value);
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

        for (const auto &expected_cycles_array : test_case_object["cycles"])
        {
            const auto &expected_operation_performed = expected_cycles_array[2];

            if (expected_operation_performed == "---")
                test_case.expected_memory_interactions.emplace_back(MemoryInteraction::None);
            else
            {
                const auto &expected_address_accessed = expected_cycles_array[0];

                if (expected_operation_performed == "r-m")
                    test_case.expected_memory_interactions.emplace_back(MemoryInteraction::Read, expected_address_accessed);
                else
                {
                    const auto &expected_value_written = expected_cycles_array[1];
                    test_case.expected_memory_interactions.emplace_back(MemoryInteraction::Write, expected_address_accessed, expected_value_written);
                }
            }
        }
        json_test_cases.push_back(test_case);
    }
    return json_test_cases;
}

// The single instruction tests expect the memory to be a 64KB flat array with no internal read/write restrictions
class SingleInstructionTestMemory : public GameBoyCore::MemoryManagementUnit
{
public:
    SingleInstructionTestMemory()
        : MemoryManagementUnit{get_timer(), get_pixel_processing_unit()}
    {
        flat_memory = std::make_unique<uint8_t[]>(GameBoyCore::MEMORY_SIZE);
        std::fill_n(flat_memory.get(), GameBoyCore::MEMORY_SIZE, 0);
    }

    void reset_state() override
    {
        std::fill_n(flat_memory.get(), GameBoyCore::MEMORY_SIZE, 0);
    }

    uint8_t read_byte(uint16_t address, bool _ = false) const override
    {
        return flat_memory[address];
    }

    void write_byte(uint16_t address, uint8_t value, bool _ = false) override
    {
        flat_memory[address] = value;
    }

private:
    std::unique_ptr<uint8_t[]> flat_memory;

    static GameBoyCore::InternalTimer &get_timer()
    {
        static GameBoyCore::InternalTimer test_internal_timer{[](uint8_t) {}};
        return test_internal_timer;
    }

    static GameBoyCore::PixelProcessingUnit &get_pixel_processing_unit()
    {
        static GameBoyCore::PixelProcessingUnit test_pixel_processing_unit{[](uint8_t) {}};
        return test_pixel_processing_unit;
    }
};

class SingleInstructionTestCentralProcessingUnit : public GameBoyCore::CentralProcessingUnit
{
public:
    std::unique_ptr<SingleInstructionTestMemory> memory_management_unit;
    std::function<void(MachineCycleOperation)> machine_cycle_memory_operation_callback;

    SingleInstructionTestCentralProcessingUnit(std::unique_ptr<SingleInstructionTestMemory> memory, std::function<void()> no_memory_operation_callback, std::function<void(MachineCycleOperation)> memory_operation_callback)
        : CentralProcessingUnit{no_memory_operation_callback, *memory},
          memory_management_unit{std::move(memory)},
          machine_cycle_memory_operation_callback{memory_operation_callback}
    {
    }

    uint8_t read_byte_and_step_emulator_components(uint16_t address) override
    {
        machine_cycle_memory_operation_callback(MachineCycleOperation{MemoryInteraction::Read, address});
        return memory_management_unit->read_byte(address);
    }

    void write_byte_and_step_emulator_components(uint16_t address, uint8_t value) override
    {
        machine_cycle_memory_operation_callback(MachineCycleOperation{MemoryInteraction::Write, address, value});
        memory_management_unit->write_byte(address, value);
    }
};

class SingleInstructionTest : public testing::TestWithParam<std::filesystem::path>
{
protected:
    std::vector<MachineCycleOperation> machine_cycle_operations;
    std::unique_ptr<SingleInstructionTestCentralProcessingUnit> game_boy_central_processing_unit;

    SingleInstructionTest()
    {
        std::unique_ptr<SingleInstructionTestMemory> memory_management_unit = std::make_unique<SingleInstructionTestMemory>();

        game_boy_central_processing_unit = std::make_unique<SingleInstructionTestCentralProcessingUnit>
        (
            std::move(memory_management_unit),
            [this]() { this->machine_cycle_operations.emplace_back(MemoryInteraction::None); },
            [this](MachineCycleOperation memory_operation) { this->machine_cycle_operations.push_back(memory_operation); }
        );
    }

    void set_initial_values(const SingleInstructionTestCase &test_case)
    {
        machine_cycle_operations.clear();
        game_boy_central_processing_unit->memory_management_unit->reset_state();
        game_boy_central_processing_unit->reset_state(false);

        for (const std::pair<uint16_t, uint8_t> &pair : test_case.initial_ram_address_value_pairs)
        {
            game_boy_central_processing_unit->memory_management_unit->write_byte(pair.first, pair.second);
        }
        game_boy_central_processing_unit->set_register_file_state(test_case.initial_register_values);
    }
};

TEST_P(SingleInstructionTest, JsonTestCasesFile)
{
    const std::filesystem::path single_instruction_json_test_file_path = GetParam();

    SCOPED_TRACE("Test file: " + single_instruction_json_test_file_path.string());
    auto test_cases_for_one_instruction = load_test_cases_from_json_file(single_instruction_json_test_file_path);

    for (size_t i = 0; i < test_cases_for_one_instruction.size(); i++)
    {
        const auto &test_case = test_cases_for_one_instruction.at(i);
        SCOPED_TRACE("Test name: " + test_case.test_name);

        set_initial_values(test_case);
        game_boy_central_processing_unit->step_single_instruction(); // Execute initial NOP (no operation) and fetch first instruction
        game_boy_central_processing_unit->step_single_instruction();

        EXPECT_EQ(game_boy_central_processing_unit->get_register_file().af, test_case.expected_register_values.af);
        EXPECT_EQ(game_boy_central_processing_unit->get_register_file().bc, test_case.expected_register_values.bc);
        EXPECT_EQ(game_boy_central_processing_unit->get_register_file().de, test_case.expected_register_values.de);
        EXPECT_EQ(game_boy_central_processing_unit->get_register_file().hl, test_case.expected_register_values.hl);

        // Compare expected program_counter against program_counter-1 since next instruction is 
        // fetched at the end of the current one, advancing the program counter an extra time
        EXPECT_EQ(static_cast<uint16_t>(game_boy_central_processing_unit->get_register_file().program_counter - 1),
                  test_case.expected_register_values.program_counter);

        EXPECT_EQ(game_boy_central_processing_unit->get_register_file().stack_pointer, test_case.expected_register_values.stack_pointer);

        for (const std::pair<uint16_t, uint8_t> &expected_pair : test_case.expected_ram_address_value_pairs)
        {
            EXPECT_EQ(game_boy_central_processing_unit->memory_management_unit->read_byte(expected_pair.first), expected_pair.second);
        }

        // Compare expected_memory_interactions size against machine_cycle_operations.size()-1 since 
        // since next instruction is fetched at the end of the current one, adding an additional read
        EXPECT_EQ(machine_cycle_operations.size() - 1, test_case.expected_memory_interactions.size());
        for (int i = 0; i < test_case.expected_memory_interactions.size(); i++)
        {
            EXPECT_EQ(machine_cycle_operations.at(i), test_case.expected_memory_interactions.at(i));
        }
    }
}

INSTANTIATE_TEST_SUITE_P
(
    SingleInstructionTestSuite,
    SingleInstructionTest,
    testing::ValuesIn(get_ordered_json_test_file_paths()),
    [](auto info)
    {
        std::string test_rom_file_name = info.param.stem().string();
        std::replace(test_rom_file_name.begin(), test_rom_file_name.end(), ' ', '_');
        return "opcode_" + test_rom_file_name;
    }
);
