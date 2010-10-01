//===-- CommandObjectSettings.cpp -------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "CommandObjectSettings.h"

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Interpreter/CommandInterpreter.h"
#include "lldb/Interpreter/CommandReturnObject.h"
#include "lldb/Interpreter/CommandCompletions.h"

using namespace lldb;
using namespace lldb_private;

//-------------------------------------------------------------------------
// CommandObjectMultiwordSettings
//-------------------------------------------------------------------------

CommandObjectMultiwordSettings::CommandObjectMultiwordSettings (CommandInterpreter &interpreter) :
    CommandObjectMultiword (interpreter,
                            "settings",
                            "A set of commands for manipulating internal settable debugger variables.",
                            "settings <command> [<command-options>]")
{
    bool status;

    CommandObjectSP set_command_object (new CommandObjectSettingsSet (interpreter));
    CommandObjectSP show_command_object (new CommandObjectSettingsShow (interpreter));
    CommandObjectSP list_command_object (new CommandObjectSettingsList (interpreter));
    CommandObjectSP remove_command_object (new CommandObjectSettingsRemove (interpreter));
    CommandObjectSP replace_command_object (new CommandObjectSettingsReplace (interpreter));
    CommandObjectSP insert_before_command_object (new CommandObjectSettingsInsertBefore (interpreter));
    CommandObjectSP insert_after_command_object (new CommandObjectSettingsInsertAfter(interpreter));
    CommandObjectSP append_command_object (new CommandObjectSettingsAppend(interpreter));
    CommandObjectSP clear_command_object (new CommandObjectSettingsClear(interpreter));

    status = LoadSubCommand ("set",           set_command_object);
    status = LoadSubCommand ("show",          show_command_object);
    status = LoadSubCommand ("list",          list_command_object);
    status = LoadSubCommand ("remove",        remove_command_object);
    status = LoadSubCommand ("replace",       replace_command_object);
    status = LoadSubCommand ("insert-before", insert_before_command_object);
    status = LoadSubCommand ("insert-after",  insert_after_command_object);
    status = LoadSubCommand ("append",        append_command_object);
    status = LoadSubCommand ("clear",         clear_command_object);
}

CommandObjectMultiwordSettings::~CommandObjectMultiwordSettings ()
{
}

//-------------------------------------------------------------------------
// CommandObjectSettingsSet
//-------------------------------------------------------------------------

CommandObjectSettingsSet::CommandObjectSettingsSet (CommandInterpreter &interpreter) :
    CommandObject (interpreter,
                   "settings set",
                   "Set or change the value of a single debugger setting variable.",
                   "settings set [<cmd-options>] <setting-variable-name> <value>"),
    m_options ()
{
}

CommandObjectSettingsSet::~CommandObjectSettingsSet()
{
}


bool
CommandObjectSettingsSet::Execute (Args& command, CommandReturnObject &result)
{
    UserSettingsControllerSP root_settings = Debugger::GetSettingsController ();

    const int argc = command.GetArgumentCount ();

    if ((argc < 2) && (!m_options.m_reset))
    {
        result.AppendError ("'settings set' takes more arguments");
        result.SetStatus (eReturnStatusFailed);
        return false;
    }

    const char *var_name = command.GetArgumentAtIndex (0);
    std::string var_name_string;
    if ((var_name == NULL) || (var_name[0] == '\0'))
    {
        result.AppendError ("'settings set' command requires a valid variable name; No value supplied");
        result.SetStatus (eReturnStatusFailed);
        return false;
    }

    var_name_string = var_name;
    command.Shift();

    const char *var_value;
    std::string value_string;

    command.GetCommandString (value_string);
    var_value = value_string.c_str();

    if (!m_options.m_reset
        && var_value == NULL)
    {
        result.AppendError ("'settings set' command requires a valid variable value unless using '--reset' option;"
                            " No value supplied");
        result.SetStatus (eReturnStatusFailed);
    }
    else
    {
      Error err = root_settings->SetVariable (var_name_string.c_str(), 
                                              var_value, 
                                              lldb::eVarSetOperationAssign, 
                                              m_options.m_override, 
                                              m_interpreter.GetDebugger().GetInstanceName().AsCString());
        if (err.Fail ())
        {
            result.AppendError (err.AsCString());
            result.SetStatus (eReturnStatusFailed);
        }
        else
            result.SetStatus (eReturnStatusSuccessFinishNoResult);
    }

    return result.Succeeded();
}

int
CommandObjectSettingsSet::HandleArgumentCompletion (Args &input,
                                                    int &cursor_index,
                                                    int &cursor_char_position,
                                                    OptionElementVector &opt_element_vector,
                                                    int match_start_point,
                                                    int max_return_elements,
                                                    bool &word_complete,
                                                    StringList &matches)
{
    std::string completion_str (input.GetArgumentAtIndex (cursor_index));
    completion_str.erase (cursor_char_position);

    // Attempting to complete variable name
    if (cursor_index == 1)
        CommandCompletions::InvokeCommonCompletionCallbacks (m_interpreter,
                                                             CommandCompletions::eSettingsNameCompletion,
                                                             completion_str.c_str(),
                                                             match_start_point,
                                                             max_return_elements,
                                                             NULL,
                                                             word_complete,
                                                             matches);

    // Attempting to complete value
    if ((cursor_index == 2)   // Partly into the variable's value
        || (cursor_index == 1  // Or at the end of a completed valid variable name
            && matches.GetSize() == 1
            && completion_str.compare (matches.GetStringAtIndex(0)) == 0))
    {
        matches.Clear();
        lldb::UserSettingsControllerSP root_settings = Debugger::GetSettingsController();
        if (cursor_index == 1)
        {
            // The user is at the end of the variable name, which is complete and valid.
            UserSettingsController::CompleteSettingsValue (root_settings,
                                                           input.GetArgumentAtIndex (1), // variable name
                                                           NULL,                         // empty value string
                                                           word_complete,
                                                           matches);
        }
        else
        {
            // The user is partly into the variable value.
            UserSettingsController::CompleteSettingsValue (root_settings,
                                                           input.GetArgumentAtIndex (1),  // variable name
                                                           completion_str.c_str(),        // partial value string
                                                           word_complete,
                                                           matches);
        }
    }

    return matches.GetSize();
}

//-------------------------------------------------------------------------
// CommandObjectSettingsSet::CommandOptions
//-------------------------------------------------------------------------

CommandObjectSettingsSet::CommandOptions::CommandOptions () :
    Options (),
    m_override (true),
    m_reset (false)
{
}

CommandObjectSettingsSet::CommandOptions::~CommandOptions ()
{
}

lldb::OptionDefinition
CommandObjectSettingsSet::CommandOptions::g_option_table[] =
{
    { LLDB_OPT_SET_1, false, "no-override", 'n', no_argument, NULL, NULL, eArgTypeNone, "Prevents already existing instances and pending settings from being assigned this new value.  Using this option means that only the default or specified instance setting values will be updated." },
    { LLDB_OPT_SET_2, false, "reset", 'r', no_argument,   NULL, NULL, eArgTypeNone, "Causes value to be reset to the original default for this variable.  No value needs to be specified when this option is used." },
    { 0, false, NULL, 0, 0, NULL, 0, eArgTypeNone, NULL }
};

const lldb::OptionDefinition*
CommandObjectSettingsSet::CommandOptions::GetDefinitions ()
{
    return g_option_table;
}

Error
CommandObjectSettingsSet::CommandOptions::SetOptionValue (int option_idx, const char *option_arg)
{
    Error error;
    char short_option = (char) m_getopt_table[option_idx].val;

    switch (short_option)
    {
        case 'n':
            m_override = false;
            break;
        case 'r':
            m_reset = true;
            break;
        default:
            error.SetErrorStringWithFormat ("Unrecognized options '%c'.\n", short_option);
            break;
    }

    return error;
}

void
CommandObjectSettingsSet::CommandOptions::ResetOptionValues ()
{
    Options::ResetOptionValues ();
    
    m_override = true;
    m_reset = false;
}

Options *
CommandObjectSettingsSet::GetOptions ()
{
    return &m_options;
}


//-------------------------------------------------------------------------
// CommandObjectSettingsShow -- Show current values
//-------------------------------------------------------------------------

CommandObjectSettingsShow::CommandObjectSettingsShow (CommandInterpreter &interpreter) :
    CommandObject (interpreter,
                   "settings show",
                   "Show the specified internal debugger setting variable and its value, or show all the currently set variables and their values, if nothing is specified.",
                   "settings show [<setting-variable-name>]")
{
}

CommandObjectSettingsShow::~CommandObjectSettingsShow()
{
}


bool
CommandObjectSettingsShow::Execute (Args& command,
                                    CommandReturnObject &result)
{
    UserSettingsControllerSP root_settings = Debugger::GetSettingsController ();
    std::string current_prefix = root_settings->GetLevelName().AsCString();

    Error err;

    if (command.GetArgumentCount())
    {
        // The user requested to see the value of a particular variable.
        lldb::SettableVariableType var_type;
        const char *variable_name = command.GetArgumentAtIndex (0);
        StringList value = root_settings->GetVariable (variable_name, var_type,
                                                       m_interpreter.GetDebugger().GetInstanceName().AsCString(),
                                                       err);
        
        if (err.Fail ())
        {
            result.AppendError (err.AsCString());
            result.SetStatus (eReturnStatusFailed);
              
         }
        else
        {
            char *type_name = (char *) "";
            if (var_type != eSetVarTypeNone)
            {
                StreamString tmp_str;
                tmp_str.Printf (" (%s)", UserSettingsController::GetTypeString (var_type));
                type_name = (char *) tmp_str.GetData();
            }

            if (value.GetSize() == 0)
                result.AppendMessageWithFormat ("%s%s = ''\n", variable_name, type_name);
            else if (value.GetSize() == 1)
                result.AppendMessageWithFormat ("%s%s = '%s'\n", variable_name, type_name, value.GetStringAtIndex (0));
            else
            {
                result.AppendMessageWithFormat ("%s%s:\n", variable_name, type_name);
                for (unsigned i = 0, e = value.GetSize(); i != e; ++i)
                {
                    result.AppendMessageWithFormat ("  [%d]: '%s'\n", i, value.GetStringAtIndex (i));
                }
            }
            result.SetStatus (eReturnStatusSuccessFinishNoResult);
        }
    }
    else
    {
        UserSettingsController::GetAllVariableValues (m_interpreter, 
                                                      root_settings, 
                                                      current_prefix, 
                                                      result.GetOutputStream(), 
                                                      err);
        if (err.Fail ())
        {
            result.AppendError (err.AsCString());
            result.SetStatus (eReturnStatusFailed);
        }
        else
        {
            result.SetStatus (eReturnStatusSuccessFinishNoResult);
        }
    }

    return result.Succeeded();
}

int
CommandObjectSettingsShow::HandleArgumentCompletion (Args &input,
                                                     int &cursor_index,
                                                     int &cursor_char_position,
                                                     OptionElementVector &opt_element_vector,
                                                     int match_start_point,
                                                     int max_return_elements,
                                                     bool &word_complete,
                                                     StringList &matches)
{
    std::string completion_str (input.GetArgumentAtIndex (cursor_index));
    completion_str.erase (cursor_char_position);

    CommandCompletions::InvokeCommonCompletionCallbacks (m_interpreter,
                                                         CommandCompletions::eSettingsNameCompletion,
                                                         completion_str.c_str(),
                                                         match_start_point,
                                                         max_return_elements,
                                                         NULL,
                                                         word_complete,
                                                         matches);
    return matches.GetSize();
}

//-------------------------------------------------------------------------
// CommandObjectSettingsList
//-------------------------------------------------------------------------

CommandObjectSettingsList::CommandObjectSettingsList (CommandInterpreter &interpreter) :
    CommandObject (interpreter, 
                   "settings list",
                   "List and describe all the internal debugger settings variables that are available to the user to 'set' or 'show', or describe a particular variable or set of variables (by specifying the variable name or a common prefix).",
                   "settings list [<setting-name> | <setting-name-prefix>]")
{
}

CommandObjectSettingsList::~CommandObjectSettingsList()
{
}


bool
CommandObjectSettingsList::Execute (                       Args& command,
                                    CommandReturnObject &result)
{
    UserSettingsControllerSP root_settings = Debugger::GetSettingsController ();
    std::string current_prefix = root_settings->GetLevelName().AsCString();

    Error err;

    if (command.GetArgumentCount() == 0)
    {
        UserSettingsController::FindAllSettingsDescriptions (m_interpreter, 
                                                             root_settings, 
                                                             current_prefix, 
                                                             result.GetOutputStream(), 
                                                             err);
    }
    else if (command.GetArgumentCount() == 1)
    {
        const char *search_name = command.GetArgumentAtIndex (0);
        UserSettingsController::FindSettingsDescriptions (m_interpreter, 
                                                          root_settings, 
                                                          current_prefix,
                                                          search_name, 
                                                          result.GetOutputStream(), 
                                                          err);
    }
    else
    {
        result.AppendError ("Too many aguments for 'settings list' command.\n");
        result.SetStatus (eReturnStatusFailed);
        return false;
    }

    if (err.Fail ())
    {
        result.AppendError (err.AsCString());
        result.SetStatus (eReturnStatusFailed);
    }
    else
    {
        result.SetStatus (eReturnStatusSuccessFinishNoResult);
    }

    return result.Succeeded();
}

int
CommandObjectSettingsList::HandleArgumentCompletion (Args &input,
                                                     int &cursor_index,
                                                     int &cursor_char_position,
                                                     OptionElementVector &opt_element_vector,
                                                     int match_start_point,
                                                     int max_return_elements,
                                                     bool &word_complete,
                                                     StringList &matches)
{
    std::string completion_str (input.GetArgumentAtIndex (cursor_index));
    completion_str.erase (cursor_char_position);

    CommandCompletions::InvokeCommonCompletionCallbacks (m_interpreter,
                                                         CommandCompletions::eSettingsNameCompletion,
                                                         completion_str.c_str(),
                                                         match_start_point,
                                                         max_return_elements,
                                                         NULL,
                                                         word_complete,
                                                         matches);
    return matches.GetSize();
}

//-------------------------------------------------------------------------
// CommandObjectSettingsRemove
//-------------------------------------------------------------------------

CommandObjectSettingsRemove::CommandObjectSettingsRemove (CommandInterpreter &interpreter) :
    CommandObject (interpreter, 
                   "settings remove",
                   "Remove the specified element from an internal debugger settings array or dictionary variable.",
                   "settings remove <setting-variable-name> [<index>|\"key\"]")
{
}

CommandObjectSettingsRemove::~CommandObjectSettingsRemove ()
{
}

bool
CommandObjectSettingsRemove::Execute (                        Args& command,
                                     CommandReturnObject &result)
{
    UserSettingsControllerSP root_settings = Debugger::GetSettingsController ();

    const int argc = command.GetArgumentCount ();

    if (argc != 2)
    {
        result.AppendError ("'settings remove' takes two arguments");
        result.SetStatus (eReturnStatusFailed);
        return false;
    }

    const char *var_name = command.GetArgumentAtIndex (0);
    std::string var_name_string;
    if ((var_name == NULL) || (var_name[0] == '\0'))
    {
        result.AppendError ("'settings remove' command requires a valid variable name; No value supplied");
        result.SetStatus (eReturnStatusFailed);
        return false;
    }

    var_name_string = var_name;
    command.Shift();

    const char *index_value = command.GetArgumentAtIndex (0);
    std::string index_value_string;
    if ((index_value == NULL) || (index_value[0] == '\0'))
    {
        result.AppendError ("'settings remove' command requires an index or key value; no value supplied");
        result.SetStatus (eReturnStatusFailed);
        return false;
    }

    index_value_string = index_value;

    Error err = root_settings->SetVariable (var_name_string.c_str(), 
                                            NULL, 
                                            lldb::eVarSetOperationRemove,  
                                            true, 
                                            m_interpreter.GetDebugger().GetInstanceName().AsCString(),
                                            index_value_string.c_str());
    if (err.Fail ())
    {
        result.AppendError (err.AsCString());
        result.SetStatus (eReturnStatusFailed);
    }
    else
        result.SetStatus (eReturnStatusSuccessFinishNoResult);

    return result.Succeeded();
}

int
CommandObjectSettingsRemove::HandleArgumentCompletion (Args &input,
                                                       int &cursor_index,
                                                       int &cursor_char_position,
                                                       OptionElementVector &opt_element_vector,
                                                       int match_start_point,
                                                       int max_return_elements,
                                                       bool &word_complete,
                                                       StringList &matches)
{
    std::string completion_str (input.GetArgumentAtIndex (cursor_index));
    completion_str.erase (cursor_char_position);

    // Attempting to complete variable name
    if (cursor_index < 2)
        CommandCompletions::InvokeCommonCompletionCallbacks (m_interpreter,
                                                             CommandCompletions::eSettingsNameCompletion,
                                                             completion_str.c_str(),
                                                             match_start_point,
                                                             max_return_elements,
                                                             NULL,
                                                             word_complete,
                                                             matches);

    return matches.GetSize();
}

//-------------------------------------------------------------------------
// CommandObjectSettingsReplace
//-------------------------------------------------------------------------

CommandObjectSettingsReplace::CommandObjectSettingsReplace (CommandInterpreter &interpreter) :
    CommandObject (interpreter,
                   "settings replace",
                   "Replace the specified element from an internal debugger settings array or dictionary variable with the specified new value.",
                   "settings replace <setting-variable-name> [<index>|\"<key>\"] <new-value>")
{
}

CommandObjectSettingsReplace::~CommandObjectSettingsReplace ()
{
}

bool
CommandObjectSettingsReplace::Execute (                         Args& command,
                                      CommandReturnObject &result)
{
    UserSettingsControllerSP root_settings = Debugger::GetSettingsController ();

    const int argc = command.GetArgumentCount ();

    if (argc < 3)
    {
        result.AppendError ("'settings replace' takes more arguments");
        result.SetStatus (eReturnStatusFailed);
        return false;
    }

    const char *var_name = command.GetArgumentAtIndex (0);
    std::string var_name_string;
    if ((var_name == NULL) || (var_name[0] == '\0'))
    {
        result.AppendError ("'settings replace' command requires a valid variable name; No value supplied");
        result.SetStatus (eReturnStatusFailed);
        return false;
    }

    var_name_string = var_name;
    command.Shift();

    const char *index_value = command.GetArgumentAtIndex (0);
    std::string index_value_string;
    if ((index_value == NULL) || (index_value[0] == '\0'))
    {
        result.AppendError ("'settings insert-before' command requires an index value; no value supplied");
        result.SetStatus (eReturnStatusFailed);
        return false;
    }

    index_value_string = index_value;
    command.Shift();

    const char *var_value;
    std::string value_string;

    command.GetCommandString (value_string);
    var_value = value_string.c_str();

    if ((var_value == NULL) || (var_value[0] == '\0'))
    {
        result.AppendError ("'settings replace' command requires a valid variable value; no value supplied");
        result.SetStatus (eReturnStatusFailed);
    }
    else
    {
        Error err = root_settings->SetVariable (var_name_string.c_str(), 
                                                var_value, 
                                                lldb::eVarSetOperationReplace, 
                                                true, 
                                                m_interpreter.GetDebugger().GetInstanceName().AsCString(),
                                                index_value_string.c_str());
        if (err.Fail ())
        {
            result.AppendError (err.AsCString());
            result.SetStatus (eReturnStatusFailed);
        }
        else
            result.SetStatus (eReturnStatusSuccessFinishNoResult);
    }

    return result.Succeeded();
}

int
CommandObjectSettingsReplace::HandleArgumentCompletion (Args &input,
                                                        int &cursor_index,
                                                        int &cursor_char_position,
                                                        OptionElementVector &opt_element_vector,
                                                        int match_start_point,
                                                        int max_return_elements,
                                                        bool &word_complete,
                                                        StringList &matches)
{
    std::string completion_str (input.GetArgumentAtIndex (cursor_index));
    completion_str.erase (cursor_char_position);

    // Attempting to complete variable name
    if (cursor_index < 2)
        CommandCompletions::InvokeCommonCompletionCallbacks (m_interpreter,
                                                             CommandCompletions::eSettingsNameCompletion,
                                                             completion_str.c_str(),
                                                             match_start_point,
                                                             max_return_elements,
                                                             NULL,
                                                             word_complete,
                                                             matches);

    return matches.GetSize();
}

//-------------------------------------------------------------------------
// CommandObjectSettingsInsertBefore
//-------------------------------------------------------------------------

CommandObjectSettingsInsertBefore::CommandObjectSettingsInsertBefore (CommandInterpreter &interpreter) :
    CommandObject (interpreter,
                   "settings insert-before",
                   "Insert value(s) into an internal debugger settings array variable, immediately before the specified element.",
                   "settings insert-before <setting-variable-name> [<index>] <new-value>")
{
}

CommandObjectSettingsInsertBefore::~CommandObjectSettingsInsertBefore ()
{
}

bool
CommandObjectSettingsInsertBefore::Execute (                              Args& command,
                                           CommandReturnObject &result)
{
    UserSettingsControllerSP root_settings = Debugger::GetSettingsController ();

    const int argc = command.GetArgumentCount ();

    if (argc < 3)
    {
        result.AppendError ("'settings insert-before' takes more arguments");
        result.SetStatus (eReturnStatusFailed);
        return false;
    }

    const char *var_name = command.GetArgumentAtIndex (0);
    std::string var_name_string;
    if ((var_name == NULL) || (var_name[0] == '\0'))
    {
        result.AppendError ("'settings insert-before' command requires a valid variable name; No value supplied");
        result.SetStatus (eReturnStatusFailed);
        return false;
    }

    var_name_string = var_name;
    command.Shift();

    const char *index_value = command.GetArgumentAtIndex (0);
    std::string index_value_string;
    if ((index_value == NULL) || (index_value[0] == '\0'))
    {
        result.AppendError ("'settings insert-before' command requires an index value; no value supplied");
        result.SetStatus (eReturnStatusFailed);
        return false;
    }

    index_value_string = index_value;
    command.Shift();

    const char *var_value;
    std::string value_string;

    command.GetCommandString (value_string);
    var_value = value_string.c_str();

    if ((var_value == NULL) || (var_value[0] == '\0'))
    {
        result.AppendError ("'settings insert-before' command requires a valid variable value;"
                            " No value supplied");
        result.SetStatus (eReturnStatusFailed);
    }
    else
    {
        Error err = root_settings->SetVariable (var_name_string.c_str(), 
                                                var_value, 
                                                lldb::eVarSetOperationInsertBefore,
                                                true, 
                                                m_interpreter.GetDebugger().GetInstanceName().AsCString(),
                                                index_value_string.c_str());
        if (err.Fail ())
        {
            result.AppendError (err.AsCString());
            result.SetStatus (eReturnStatusFailed);
        }
        else
            result.SetStatus (eReturnStatusSuccessFinishNoResult);
    }

    return result.Succeeded();
}


int
CommandObjectSettingsInsertBefore::HandleArgumentCompletion (Args &input,
                                                             int &cursor_index,
                                                             int &cursor_char_position,
                                                             OptionElementVector &opt_element_vector,
                                                             int match_start_point,
                                                             int max_return_elements,
                                                             bool &word_complete,
                                                             StringList &matches)
{
    std::string completion_str (input.GetArgumentAtIndex (cursor_index));
    completion_str.erase (cursor_char_position);

    // Attempting to complete variable name
    if (cursor_index < 2)
        CommandCompletions::InvokeCommonCompletionCallbacks (m_interpreter,
                                                             CommandCompletions::eSettingsNameCompletion,
                                                             completion_str.c_str(),
                                                             match_start_point,
                                                             max_return_elements,
                                                             NULL,
                                                             word_complete,
                                                             matches);

    return matches.GetSize();
}

//-------------------------------------------------------------------------
// CommandObjectSettingInsertAfter
//-------------------------------------------------------------------------

CommandObjectSettingsInsertAfter::CommandObjectSettingsInsertAfter (CommandInterpreter &interpreter) :
    CommandObject (interpreter,
                   "settings insert-after",
                   "Insert value(s) into an internal debugger settings array variable, immediately after the specified element.",
                   "settings insert-after <setting-variable-name> [<index>] <new-value>")
{
}

CommandObjectSettingsInsertAfter::~CommandObjectSettingsInsertAfter ()
{
}

bool
CommandObjectSettingsInsertAfter::Execute (                             Args& command,
                                          CommandReturnObject &result)
{
    UserSettingsControllerSP root_settings = Debugger::GetSettingsController ();

    const int argc = command.GetArgumentCount ();

    if (argc < 3)
    {
        result.AppendError ("'settings insert-after' takes more arguments");
        result.SetStatus (eReturnStatusFailed);
        return false;
    }

    const char *var_name = command.GetArgumentAtIndex (0);
    std::string var_name_string;
    if ((var_name == NULL) || (var_name[0] == '\0'))
    {
        result.AppendError ("'settings insert-after' command requires a valid variable name; No value supplied");
        result.SetStatus (eReturnStatusFailed);
        return false;
    }

    var_name_string = var_name;
    command.Shift();

    const char *index_value = command.GetArgumentAtIndex (0);
    std::string index_value_string;
    if ((index_value == NULL) || (index_value[0] == '\0'))
    {
        result.AppendError ("'settings insert-after' command requires an index value; no value supplied");
        result.SetStatus (eReturnStatusFailed);
        return false;
    }

    index_value_string = index_value;
    command.Shift();

    const char *var_value;
    std::string value_string;

    command.GetCommandString (value_string);
    var_value = value_string.c_str();

    if ((var_value == NULL) || (var_value[0] == '\0'))
    {
        result.AppendError ("'settings insert-after' command requires a valid variable value;"
                            " No value supplied");
        result.SetStatus (eReturnStatusFailed);
    }
    else
    {
        Error err = root_settings->SetVariable (var_name_string.c_str(), 
                                                var_value, 
                                                lldb::eVarSetOperationInsertAfter,
                                                true, 
                                                m_interpreter.GetDebugger().GetInstanceName().AsCString(), 
                                                index_value_string.c_str());
        if (err.Fail ())
        {
            result.AppendError (err.AsCString());
            result.SetStatus (eReturnStatusFailed);
        }
        else
            result.SetStatus (eReturnStatusSuccessFinishNoResult);
    }

    return result.Succeeded();
}


int
CommandObjectSettingsInsertAfter::HandleArgumentCompletion (Args &input,
                                                            int &cursor_index,
                                                            int &cursor_char_position,
                                                            OptionElementVector &opt_element_vector,
                                                            int match_start_point,
                                                            int max_return_elements,
                                                            bool &word_complete,
                                                            StringList &matches)
{
    std::string completion_str (input.GetArgumentAtIndex (cursor_index));
    completion_str.erase (cursor_char_position);

    // Attempting to complete variable name
    if (cursor_index < 2)
        CommandCompletions::InvokeCommonCompletionCallbacks (m_interpreter,
                                                             CommandCompletions::eSettingsNameCompletion,
                                                             completion_str.c_str(),
                                                             match_start_point,
                                                             max_return_elements,
                                                             NULL,
                                                             word_complete,
                                                             matches);

    return matches.GetSize();
}

//-------------------------------------------------------------------------
// CommandObjectSettingsAppend
//-------------------------------------------------------------------------

CommandObjectSettingsAppend::CommandObjectSettingsAppend (CommandInterpreter &interpreter) :
    CommandObject (interpreter,
                   "settings append",
                   "Append a new value to the end of an internal debugger settings array, dictionary or string variable.",
                   "settings append <setting-variable-name> <new-value>")
{
}

CommandObjectSettingsAppend::~CommandObjectSettingsAppend ()
{
}

bool
CommandObjectSettingsAppend::Execute (Args& command,
                                      CommandReturnObject &result)
{
    UserSettingsControllerSP root_settings = Debugger::GetSettingsController ();

    const int argc = command.GetArgumentCount ();

    if (argc < 2)
    {
        result.AppendError ("'settings append' takes more arguments");
        result.SetStatus (eReturnStatusFailed);
        return false;
    }

    const char *var_name = command.GetArgumentAtIndex (0);
    std::string var_name_string;
    if ((var_name == NULL) || (var_name[0] == '\0'))
    {
        result.AppendError ("'settings append' command requires a valid variable name; No value supplied");
        result.SetStatus (eReturnStatusFailed);
        return false;
    }

    var_name_string = var_name;
    command.Shift();

    const char *var_value;
    std::string value_string;

    command.GetCommandString (value_string);
    var_value = value_string.c_str();

    if ((var_value == NULL) || (var_value[0] == '\0'))
    {
        result.AppendError ("'settings append' command requires a valid variable value;"
                            " No value supplied");
        result.SetStatus (eReturnStatusFailed);
    }
    else
    {
        Error err = root_settings->SetVariable (var_name_string.c_str(), 
                                                var_value, 
                                                lldb::eVarSetOperationAppend, 
                                                true, 
                                                m_interpreter.GetDebugger().GetInstanceName().AsCString());
        if (err.Fail ())
        {
            result.AppendError (err.AsCString());
            result.SetStatus (eReturnStatusFailed);
        }
        else
            result.SetStatus (eReturnStatusSuccessFinishNoResult);
    }

    return result.Succeeded();
}


int
CommandObjectSettingsAppend::HandleArgumentCompletion (Args &input,
                                                       int &cursor_index,
                                                       int &cursor_char_position,
                                                       OptionElementVector &opt_element_vector,
                                                       int match_start_point,
                                                       int max_return_elements,
                                                       bool &word_complete,
                                                       StringList &matches)
{
    std::string completion_str (input.GetArgumentAtIndex (cursor_index));
    completion_str.erase (cursor_char_position);

    // Attempting to complete variable name
    if (cursor_index < 2)
        CommandCompletions::InvokeCommonCompletionCallbacks (m_interpreter,
                                                             CommandCompletions::eSettingsNameCompletion,
                                                             completion_str.c_str(),
                                                             match_start_point,
                                                             max_return_elements,
                                                             NULL,
                                                             word_complete,
                                                             matches);

    return matches.GetSize();
}

//-------------------------------------------------------------------------
// CommandObjectSettingsClear
//-------------------------------------------------------------------------

CommandObjectSettingsClear::CommandObjectSettingsClear (CommandInterpreter &interpreter) :
    CommandObject (interpreter, 
                   "settings clear",
                   "Erase all the contents of an internal debugger settings variables; this is only valid for variables with clearable types, i.e. strings, arrays or dictionaries.",
                   "settings clear")
{
}

CommandObjectSettingsClear::~CommandObjectSettingsClear ()
{
}

bool
CommandObjectSettingsClear::Execute (                       Args& command,
                                    CommandReturnObject &result)
{
    UserSettingsControllerSP root_settings = Debugger::GetSettingsController ();

    const int argc = command.GetArgumentCount ();

    if (argc != 1)
    {
        result.AppendError ("'setttings clear' takes exactly one argument");
        result.SetStatus (eReturnStatusFailed);
        return false;
    }

    const char *var_name = command.GetArgumentAtIndex (0);
    if ((var_name == NULL) || (var_name[0] == '\0'))
    {
        result.AppendError ("'settings clear' command requires a valid variable name; No value supplied");
        result.SetStatus (eReturnStatusFailed);
        return false;
    }

    Error err = root_settings->SetVariable (var_name, 
                                            NULL, 
                                            lldb::eVarSetOperationClear, 
                                            false, 
                                            m_interpreter.GetDebugger().GetInstanceName().AsCString());

    if (err.Fail ())
    {
        result.AppendError (err.AsCString());
        result.SetStatus (eReturnStatusFailed);
    }
    else
        result.SetStatus (eReturnStatusSuccessFinishNoResult);

    return result.Succeeded();
}


int
CommandObjectSettingsClear::HandleArgumentCompletion (Args &input,
                                                      int &cursor_index,
                                                      int &cursor_char_position,
                                                      OptionElementVector &opt_element_vector,
                                                      int match_start_point,
                                                      int max_return_elements,
                                                      bool &word_complete,
                                                      StringList &matches)
{
    std::string completion_str (input.GetArgumentAtIndex (cursor_index));
    completion_str.erase (cursor_char_position);

    // Attempting to complete variable name
    if (cursor_index < 2)
        CommandCompletions::InvokeCommonCompletionCallbacks (m_interpreter,
                                                             CommandCompletions::eSettingsNameCompletion,
                                                             completion_str.c_str(),
                                                             match_start_point,
                                                             max_return_elements,
                                                             NULL,
                                                             word_complete,
                                                             matches);

    return matches.GetSize();
}
