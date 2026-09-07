#pragma once
#include "../include/metasequoia/session.h"
#include "../quanpin/quanpin_dictionary.h"

namespace metasequoia
{
// Owns ambiguous digit composition; platforms only send keys and choose snapshot indices.
class NineKeySession
{
  public:
    explicit NineKeySession(RuntimePaths paths) : paths_(std::move(paths))
    {
    }
    bool active() const
    {
        return !digits_.empty();
    }
    KeyResult character(char digit);
    KeyResult command(Command command);
    KeyResult select(std::size_t index);
    KeyResult finish(std::size_t index);
    KeyResult choose_spelling(std::size_t index);
    SessionSnapshot snapshot() const;

  private:
    void refresh();
    std::size_t locked_length() const;
    RuntimePaths paths_;
    std::unique_ptr<QuanpinDictionary> dictionary_;
    std::string digits_;
    std::vector<std::string> locked_;
    std::vector<std::string> spellings_;
    std::vector<WordItem> candidates_;
};
} // namespace metasequoia
