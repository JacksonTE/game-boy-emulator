#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <vector>
#include "game_boy.h"
#include "memory_interface.h"
#include "register_file.h"

struct AddressValuePair {
    uint16_t memory_address;
    uint8_t value;
};

struct JsonTestCase {
    std::string test_name;

    GameBoy::RegisterFile<std::endian::native> register_values_initial;
    std::vector<AddressValuePair> ram_values_initial;

    GameBoy::RegisterFile<std::endian::native> register_values_expected_final;
    std::vector<AddressValuePair> ram_values_expected_final;
};

static std::vector<std::filesystem::path> get_json_test_file_paths(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> filenames;

    for (const auto &entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
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

static std::vector<JsonTestCase> load_json_test_cases(const std::filesystem::path& json_tests_file_path) {
    std::ifstream file(json_tests_file_path);
    if (!file) {
        throw std::runtime_error("Error: could not open JSON file " + json_tests_file_path.string());
    }

    nlohmann::json test_file_json;
    file >> test_file_json;
    std::vector<JsonTestCase> json_test_cases;

    for (const auto &test_object : test_file_json) {
        JsonTestCase test_case;

        test_case.test_name = test_object["name"].get<std::string>();

        for (const auto &initial_ram_array : test_object["initial"]["ram"]) {
            test_case.ram_values_initial.emplace_back(AddressValuePair{ initial_ram_array[0], initial_ram_array[1] });
        }
        test_case.register_values_initial.program_counter = test_object["initial"]["pc"].get<uint16_t>();
        test_case.register_values_initial.stack_pointer = test_object["initial"]["sp"].get<uint16_t>();
        test_case.register_values_initial.a = test_object["initial"]["a"].get<uint8_t>();
        test_case.register_values_initial.b = test_object["initial"]["b"].get<uint8_t>();
        test_case.register_values_initial.c = test_object["initial"]["c"].get<uint8_t>();
        test_case.register_values_initial.d = test_object["initial"]["d"].get<uint8_t>();
        test_case.register_values_initial.e = test_object["initial"]["e"].get<uint8_t>();
        test_case.register_values_initial.flags = test_object["initial"]["f"].get<uint8_t>();
        test_case.register_values_initial.h = test_object["initial"]["h"].get<uint8_t>();
        test_case.register_values_initial.l = test_object["initial"]["l"].get<uint8_t>();

        for (const auto &expected_final_ram_array : test_object["final"]["ram"]) {
            test_case.ram_values_expected_final.emplace_back(AddressValuePair{ expected_final_ram_array[0], expected_final_ram_array[1] });
        }
        test_case.register_values_expected_final.program_counter = test_object["final"]["pc"].get<uint16_t>();
        test_case.register_values_expected_final.stack_pointer = test_object["final"]["sp"].get<uint16_t>();
        test_case.register_values_expected_final.a = test_object["final"]["a"].get<uint8_t>();
        test_case.register_values_expected_final.b = test_object["final"]["b"].get<uint8_t>();
        test_case.register_values_expected_final.c = test_object["final"]["c"].get<uint8_t>();
        test_case.register_values_expected_final.d = test_object["final"]["d"].get<uint8_t>();
        test_case.register_values_expected_final.e = test_object["final"]["e"].get<uint8_t>();
        test_case.register_values_expected_final.flags = test_object["final"]["f"].get<uint8_t>();
        test_case.register_values_expected_final.h = test_object["final"]["h"].get<uint8_t>();
        test_case.register_values_expected_final.l = test_object["final"]["l"].get<uint8_t>();

        json_test_cases.push_back(test_case);
    }
    return json_test_cases;
}

class JsonTestMemory : public GameBoy::MemoryInterface {
public:
    JsonTestMemory() {
        std::cout << "test constructor" << "\n";
        memory.fill(0);
    }

    void reset() override {
        memory.fill(0);
    }

    void set_post_boot_state() override {
        // Implementation not necessary
    }

    bool try_load_file(uint16_t address, uint32_t number_of_bytes_to_load, std::filesystem::path file_path, bool is_bootrom_file) override {
        // Implementation not necessary
        return true;
    }

    uint8_t read_8(uint16_t address) const override {
        return memory[address];
    }

    void write_8(uint16_t address, uint8_t value) override {
        memory[address] = value;
    }

private:
    std::array<uint8_t, GameBoy::MEMORY_SIZE> memory;
};

class JsonTest : public testing::TestWithParam<std::filesystem::path> {
protected:
    GameBoy::GameBoy game_boy;

    JsonTest() : game_boy{ std::make_unique<JsonTestMemory>() } {} // TODO fix this not populating correctly

    void set_initial_values(const JsonTestCase &test_case) {
        game_boy.reset();

        for (const AddressValuePair &pair : test_case.ram_values_initial) {
            game_boy.write_byte_to_memory(pair.memory_address, pair.value);
        }
        game_boy.update_register_file(test_case.register_values_initial);
    }
};

TEST_P(JsonTest, SingleInstructionTestFile) {
    std::filesystem::path json_test_filename = GetParam();
    SCOPED_TRACE("Test file: " + json_test_filename.string());

    auto test_cases = load_json_test_cases(json_test_filename);

    for (const auto &test_case : test_cases) {
        SCOPED_TRACE("Test name: " + test_case.test_name);

        set_initial_values(test_case);
        game_boy.execute_next_instruction();

        GameBoy::RegisterFile<std::endian::native> final_register_values = game_boy.get_register_file();
        EXPECT_EQ(final_register_values.af, test_case.register_values_expected_final.af);
        EXPECT_EQ(final_register_values.bc, test_case.register_values_expected_final.bc);
        EXPECT_EQ(final_register_values.de, test_case.register_values_expected_final.de);
        EXPECT_EQ(final_register_values.hl, test_case.register_values_expected_final.hl);
        EXPECT_EQ(final_register_values.program_counter, test_case.register_values_expected_final.program_counter);
        EXPECT_EQ(final_register_values.stack_pointer, test_case.register_values_expected_final.stack_pointer);

        for (const AddressValuePair &pair : test_case.ram_values_expected_final) {
            EXPECT_EQ(game_boy.read_byte_from_memory(pair.memory_address), pair.value);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    JsonTestSuite,
    JsonTest,
    testing::ValuesIn(get_json_test_file_paths(std::filesystem::path("..") / ".." / ".." / ".." / "tests" / "data" / "sm83" / "v1"))
);

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
