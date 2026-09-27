// tests/p2/test_p2.cpp
// Assert-based tests for ECE 309 Project 2.

#include "core/conversation.h"
#include "core/message.h"
#include "core/sentinel_scanner.h"
#include "harness/harness.h"
#include "model/replay_client.h"
#include "model/scripted_client.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

const std::string kSentinel = "<|end_conversation|>";

class FixedInput : public InputSource {
public:
    explicit FixedInput(std::vector<std::string> lines) : lines_(std::move(lines)) {}

    std::string read_line() override {
        if (next_line_ == lines_.size()) {
            eof_ = true;
            return "";
        }
        return lines_[next_line_++];
    }

    bool is_eof() const override { return eof_; }

private:
    std::vector<std::string> lines_;
    std::size_t next_line_ = 0;
    bool eof_ = false;
};

class CapturingOutput : public OutputSink {
public:
    void write(std::string_view text) override { text_ += text; }

    const std::string& text() const noexcept { return text_; }

private:
    std::string text_;
};

std::filesystem::path temporary_path(const std::string& name) {
    return std::filesystem::temp_directory_path() / ("ece309_p2_" + name);
}

void write_file(const std::filesystem::path& path, const std::string& text) {
    std::ofstream file(path);
    assert(file.is_open());
    file << text;
    file.close();
    assert(file.good());
}

const char* role_name(Role role) {
    switch (role) {
        case Role::System: return "system";
        case Role::User: return "user";
        case Role::Assistant: return "assistant";
    }
    return "assistant";
}

void save_transcript_for_test(const Conversation& conversation,
                              const std::filesystem::path& path) {
    std::ofstream file(path);
    assert(file.is_open());

    bool first = true;
    for (const Message* message = conversation.begin();
         message != conversation.end(); ++message) {
        if (!first) file << "---\n";
        first = false;
        file << "role: " << role_name(message->role()) << "\n";
        file << message->content() << "\n";
    }

    file.close();
    assert(file.good());
}

void test_message_construction() {
    Message blank;
    assert(blank.role() == Role::System);
    assert(blank.content().empty());

    Message reply(Role::Assistant, "hello");
    assert(reply.role() == Role::Assistant);
    assert(reply.content() == "hello");
}

void test_empty_conversation_bounds() {
    Conversation conversation;
    assert(conversation.size() == 0);
    assert(conversation.begin() == conversation.end());

    bool threw = false;
    try {
        (void)conversation.at(0);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    assert(threw);
}

void test_system_message_ordering() {
    Conversation conversation;
    conversation.append(Message(Role::System, "Keep answers short."));
    conversation.append(Message(Role::User, "hello"));
    conversation.append(Message(Role::Assistant, "Hi."));

    assert(conversation.size() == 3);
    assert(conversation.at(0).role() == Role::System);
    assert(conversation.at(0).content() == "Keep answers short.");
    assert(conversation.at(1).role() == Role::User);
    assert(conversation.at(2).role() == Role::Assistant);
}

void test_copy_constructor_is_deep() {
    Conversation original;
    original.append(Message(Role::User, "first"));
    original.append(Message(Role::Assistant, "second"));

    Conversation copy(original);
    assert(copy.size() == original.size());
    assert(copy.begin() != original.begin());
    assert(copy.at(0).content() == "first");
    assert(copy.at(1).content() == "second");
}

void test_copy_assignment_is_deep_and_self_safe() {
    Conversation original;
    original.append(Message(Role::User, "question"));

    Conversation assigned;
    assigned.append(Message(Role::Assistant, "old content"));
    assigned = original;
    assert(assigned.size() == 1);
    assert(assigned.begin() != original.begin());
    assert(assigned.at(0).content() == "question");

    assigned = assigned;
    assert(assigned.size() == 1);
    assert(assigned.at(0).content() == "question");
}

void test_move_constructor_steals_storage() {
    Conversation source;
    source.append(Message(Role::User, "move me"));
    const Message* source_storage = source.begin();

    Conversation moved(std::move(source));
    assert(moved.begin() == source_storage);
    assert(moved.size() == 1);
    assert(moved.at(0).content() == "move me");
    assert(source.size() == 0);
    assert(source.begin() == source.end());
}

void test_move_assignment_steals_storage() {
    Conversation source;
    source.append(Message(Role::Assistant, "new value"));
    const Message* source_storage = source.begin();

    Conversation target;
    target.append(Message(Role::User, "old value"));
    target = std::move(source);

    assert(target.begin() == source_storage);
    assert(target.size() == 1);
    assert(target.at(0).content() == "new value");
    assert(source.size() == 0);
    assert(source.begin() == source.end());
}

void test_doubling_growth_and_iteration() {
    Conversation conversation;
    conversation.append(Message(Role::User, "m0"));
    conversation.append(Message(Role::User, "m1"));
    conversation.append(Message(Role::User, "m2"));

    // With 1, 2, 4, ... capacity growth, appending the fourth message does
    // not reallocate. Its pointer must therefore stay stable.
    const Message* storage_at_capacity_four = conversation.begin();
    conversation.append(Message(Role::User, "m3"));
    assert(conversation.begin() == storage_at_capacity_four);

    assert(conversation.size() == 4);
    std::size_t index = 0;
    for (const Message& message : conversation) {
        assert(message.content() == "m" + std::to_string(index));
        ++index;
    }
    assert(index == conversation.size());
}

void test_scanner_clean_text() {
    SentinelScanner scanner(kSentinel);
    const std::string first_part = "A normal reply, ";
    const std::string second_part = "with no stop marker.";

    const auto first = scanner.feed(first_part);
    const auto second = scanner.feed(second_part);
    const auto last = scanner.flush();

    assert(!first.sentinel_found);
    assert(!second.sentinel_found);
    assert(!last.sentinel_found);
    assert(first.safe_text + second.safe_text + last.safe_text ==
           first_part + second_part);
}

void test_scanner_whole_sentinel() {
    SentinelScanner scanner(kSentinel);
    const auto result = scanner.feed("Goodbye." + kSentinel + "ignored text");

    assert(result.sentinel_found);
    assert(result.safe_text == "Goodbye.");
    assert(scanner.flush().safe_text.empty());
}

void test_scanner_catches_every_split_boundary() {
    const std::string text = "Goodbye." + kSentinel;
    for (std::size_t split = 0; split <= text.size(); ++split) {
        SentinelScanner scanner(kSentinel);
        const auto first = scanner.feed(text.substr(0, split));
        const auto second = scanner.feed(text.substr(split));

        assert(first.sentinel_found || second.sentinel_found);
        assert(first.safe_text + second.safe_text == "Goodbye.");
    }
}

void test_scanner_catches_one_character_stream() {
    SentinelScanner scanner(kSentinel);
    const std::string text = "One byte at a time: " + kSentinel;
    std::string printed;
    bool found = false;

    for (char character : text) {
        const auto result = scanner.feed(std::string_view(&character, 1));
        printed += result.safe_text;
        found = found || result.sentinel_found;
    }

    assert(found);
    assert(printed == "One byte at a time: ");
}

void test_scanner_ignores_false_alarms() {
    SentinelScanner scanner(kSentinel);
    const std::string false_alarm = "before <|end_world|> after";

    const auto result = scanner.feed(false_alarm);
    const auto final = scanner.flush();

    assert(!result.sentinel_found);
    assert(!final.sentinel_found);
    assert(result.safe_text + final.safe_text == false_alarm);
}

void test_scanner_pending_memory_is_bounded() {
    SentinelScanner scanner(kSentinel);
    const std::string adversarial = "<|end_";
    std::size_t characters_fed = 0;
    std::size_t characters_emitted = 0;

    // The un-emitted portion equals pending_ because no sentinel occurs in
    // this stream. Feeding a full 4 MiB one character at a time also checks
    // the chunk-boundary stress case from the specification.
    constexpr std::size_t kBytes = 4U * 1024U * 1024U;
    for (std::size_t i = 0; i < kBytes; ++i) {
        const char character = adversarial[i % adversarial.size()];
        const auto result = scanner.feed(std::string_view(&character, 1));
        assert(!result.sentinel_found);

        ++characters_fed;
        characters_emitted += result.safe_text.size();
        assert(characters_fed - characters_emitted <= kSentinel.size() - 1);
    }

    const auto final = scanner.flush();
    assert(!final.sentinel_found);
    assert(final.safe_text.size() == characters_fed - characters_emitted);
}

void test_harness_turn_limit() {
    const auto script = temporary_path("turn_limit.script");
    write_file(script,
               "role: assistant\n"
               "first reply\n"
               "---\n"
               "role: assistant\n"
               "second reply\n");

    HarnessConfig config;
    config.max_turns = 2;
    config.system_message = "Be concise.";
    Harness harness(std::make_unique<ScriptedModelClient>(script.string()), config);
    FixedInput input({"first question", "second question"});
    CapturingOutput output;

    const StopReason reason = harness.run(input, output);
    assert(reason.kind == StopReason::Kind::TurnLimit);
    assert(harness.conversation().size() == 5);
    assert(harness.conversation().at(0).role() == Role::System);
    assert(harness.conversation().at(4).content() == "second reply");
    assert(output.text().find("first reply") != std::string::npos);

    std::filesystem::remove(script);
}

void test_harness_stops_on_streamed_sentinel() {
    const auto script = temporary_path("sentinel.script");
    write_file(script,
               "chunk: 4\n"
               "role: assistant\n"
               "Goodbye.<|end_conversation|>discarded\n");

    HarnessConfig config;
    config.max_turns = 5;
    Harness harness(std::make_unique<ScriptedModelClient>(script.string()), config);
    FixedInput input({"bye"});
    CapturingOutput output;

    const StopReason reason = harness.run(input, output);
    assert(reason.kind == StopReason::Kind::Sentinel);
    assert(harness.conversation().size() == 2);
    assert(harness.conversation().at(1).content() == "Goodbye." + kSentinel);
    assert(output.text().find(kSentinel) == std::string::npos);

    std::filesystem::remove(script);
}

void test_harness_handles_eof() {
    const auto script = temporary_path("eof.script");
    write_file(script, "role: assistant\nunused\n");

    Harness harness(std::make_unique<ScriptedModelClient>(script.string()), {});
    FixedInput input({});
    CapturingOutput output;

    const StopReason reason = harness.run(input, output);
    assert(reason.kind == StopReason::Kind::UserExit);
    assert(harness.conversation().size() == 0);
    assert(output.text() == "you> ");

    std::filesystem::remove(script);
}

void test_transcript_round_trip_with_replay_client() {
    Conversation saved;
    saved.append(Message(Role::System, "Be helpful."));
    saved.append(Message(Role::User, "hello"));
    saved.append(Message(Role::Assistant, "Hi there."));
    saved.append(Message(Role::User, "bye"));
    saved.append(Message(Role::Assistant, "Goodbye." + kSentinel));

    const auto transcript = temporary_path("round_trip.txt");
    save_transcript_for_test(saved, transcript);

    ReplayModelClient replay(transcript.string());
    Conversation context;
    context.append(Message(Role::User, "hello"));
    const Message first = replay.generate(context);
    context.append(Message(Role::User, "bye"));
    const Message second = replay.generate(context);

    assert(replay.system_message() == "Be helpful.");
    assert(first.role() == Role::Assistant);
    assert(first.content() == "Hi there.");
    assert(second.content() == "Goodbye." + kSentinel);

    std::filesystem::remove(transcript);
}

}  // namespace

int main() {
    test_message_construction();
    test_empty_conversation_bounds();
    test_system_message_ordering();
    test_copy_constructor_is_deep();
    test_copy_assignment_is_deep_and_self_safe();
    test_move_constructor_steals_storage();
    test_move_assignment_steals_storage();
    test_doubling_growth_and_iteration();
    test_scanner_clean_text();
    test_scanner_whole_sentinel();
    test_scanner_catches_every_split_boundary();
    test_scanner_catches_one_character_stream();
    test_scanner_ignores_false_alarms();
    test_scanner_pending_memory_is_bounded();
    test_harness_turn_limit();
    test_harness_stops_on_streamed_sentinel();
    test_harness_handles_eof();
    test_transcript_round_trip_with_replay_client();
    return 0;
}
