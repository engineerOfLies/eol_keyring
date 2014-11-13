#include "eol_config.h"
#include <glib.h>
#include <yaml.h>

/*local globals*/
eolBool _eol_config_initialized;


/*local functions*/
void eol_config_deinit(void);
void eol_config_parse_tier(yaml_parser_t *parser, eolKeychain *cfg);

/*function definitions*/

void eol_config_init()
{
  _eol_config_initialized = eolTrue;
  atexit(eol_config_deinit);
}

void eol_config_deinit(void)
{
  _eol_config_initialized = eolFalse;
}

eolConfig *eol_config_new()
{
  eolConfig *config = NULL;
  config = malloc(sizeof(eolConfig));
  if (config == NULL)
  {
    return NULL;
  }
  memset(config,0,sizeof(eolConfig));
  return config;
}

void eol_config_save_binary(eolConfig *conf, char* filename)
{
  eolFile *file = NULL;
  if (!conf)return;
  /*TODO*/
}

eolConfig *eol_config_load_binary(char* filename)
{
  eolConfig *config = NULL;
  eolFile *file = NULL;
  file = eol_loader_read_file_binary(filename);
  if (file == NULL)return NULL;
  config = eol_config_new();
  if (config == NULL)
  {
    eol_loader_close_file(&file);
    return NULL;
  }
  eol_line_cpy(config->filename,filename);
  config->_node = eol_loader_read_keychain_link(file);
  eol_loader_close_file(&file);
  return config;
}

size_t eol_config_get_file_size(FILE *file)
{
  size_t char_count = 0;
  char * buffer;
  char temp[2];
  if (!file)return 0;
  while(fread(temp, sizeof(char), 1, file) != 0)
  {
    char_count++;
  }
  rewind(file);
  return char_count;
}

char *eol_get_file_buffer(size_t *osize, FILE *file)
{
  char * buffer = NULL;
  size_t size = 0;
  if (!file)return NULL;
  size = eol_config_get_file_size(file);
  if (size <= 0) return NULL;
  buffer = (char *)malloc(sizeof(char)*size);
  if (!buffer)return NULL;
  if (fread(buffer,sizeof(char),size,file) == 0)return NULL;
  if (osize)
  {
    *osize = size;
  }
  return buffer;
}

eolConfig *eol_config_load(char* filename)
{
  yaml_parser_t parser;
  char *buffer = NULL;
  size_t size = 0;
  FILE *input = NULL;
  eolConfig *config = NULL;
  
  if(!yaml_parser_initialize(&parser))
  {
    return NULL;
  }

  config= eol_config_new();
  if (config == NULL)return NULL;
  
  eol_line_cpy(config->filename, filename);
  config->_node = eol_keychain_new_hash();
  if(config->_node == NULL)
  {
    return NULL;
  }
  input = fopen(filename,"r");
  if(input == NULL)
  {
    return NULL;
  }
  /*
  
    yaml_parser_set_input_file(&parser, input->file);
  */
  
  /*TODO: test the following on alternate endianness architectures before deleting the above*/
  buffer = eol_get_file_buffer(&size,input);
  yaml_parser_set_input_string(&parser, (const unsigned char *)buffer, size);
  eol_config_parse_tier(&parser, config->_node);

  yaml_parser_delete(&parser);
  fclose(input);
  return config;
}

void eol_config_destroy(eolConfig *config)
{
  if (!config)return;
  eol_keychain_destroy(config->_node);
  free(config);
}

void eol_config_free(eolConfig **config)
{
  if (!config)return;
  if (!*config)return;
  eol_config_destroy(*config);
  *config = NULL;
}

void eol_config_parse_sequence(yaml_parser_t *parser, eolKeychain *chain)
{
  int done = 0;
  eolKeychain *next = NULL;
  yaml_event_t event;
  /* First element must be a variable, or we'll change states to SEQ */
  int state = KEY;
  do
  {
    yaml_parser_parse(parser, &event);
    switch(event.type)
    {
      case YAML_MAPPING_START_EVENT:
        next = eol_keychain_new_hash();
        eol_keychain_list_append(chain,next);
        state ^= VAL;
        eol_config_parse_tier(parser, next);
        break;
      case YAML_SEQUENCE_END_EVENT:
      case YAML_MAPPING_END_EVENT:
      case YAML_STREAM_END_EVENT:
        done = 1;
        /* terminate the while loop, see below */
        break;
      default:
    }
    if(parser->error != YAML_NO_ERROR)
    {
                          parser->error, parser->context, parser->problem, parser->problem_mark.line,
                          parser->problem_mark.column);
                          return;
    }
    yaml_event_delete(&event);
  }while (!done);
}

void eol_config_parse_tier(yaml_parser_t *parser, eolKeychain *chain)
{
  int done = 0;
  eolLine last_key;
  eolKeychain *next = NULL;
  yaml_event_t event;
  /* First element must be a variable, or we'll change states to SEQ */
  int state = KEY;
  eol_line_cpy(last_key,"");
  do
  {
    yaml_parser_parse(parser, &event);
    switch(event.type)
    {
      case YAML_SCALAR_EVENT:
        if (state == KEY)
        {
          /* new key, hold on to it until we get a value as well */
          eol_line_cpy(last_key,(char *)event.data.scalar.value);
        }
        else
        {
          /* state is VAL or SEQ */
          /* TODO data type logic should go here */
          next = eol_keychain_new_string((char *)event.data.scalar.value);
          eol_keychain_hash_insert(chain,last_key,next);
        }
        state ^= VAL; /* Toggles KEY/VAL, avoids touching SEQ */
        break;
      case YAML_SEQUENCE_START_EVENT:
        next = eol_keychain_new_list();
        eol_keychain_hash_insert(chain,last_key,
                                 next);
        eol_config_parse_sequence(parser, next);
        break;
      case YAML_MAPPING_START_EVENT:
        if (strlen(last_key) == 0)break;/*first level is implied hash.*/
        next = eol_keychain_new_hash();
        eol_keychain_hash_insert(chain,last_key,next);
        state ^= VAL;
        eol_config_parse_tier(parser, next);
        break;
      case YAML_MAPPING_END_EVENT:
      case YAML_STREAM_END_EVENT:
      case YAML_SEQUENCE_END_EVENT:
        done = 1;
        /* terminate the while loop, see below */
        break;
      default:
    }
    if(parser->error != YAML_NO_ERROR)
    {
                          parser->error, parser->context, parser->problem, parser->problem_mark.line,
                          parser->problem_mark.column);
                          return;
    }
    yaml_event_delete(&event);
  }
  while (!done);
}

eolBool eol_config_get_orientation_by_tag(
  eolOrientation  * output,
  eolConfig       * conf,
  eolLine           tag
)
{
  g_return_val_if_fail(conf, eolFalse);
  g_return_val_if_fail(output, eolFalse);
  return eol_keychain_get_hash_value_as_orientation(output, conf->_node, tag);
}


eolBool eol_config_get_keychain(eolKeychain *output,
                                eolConfig *conf)
{
  g_return_val_if_fail(conf->_node, eolFalse);
  g_return_val_if_fail(output, eolFalse);
  output = conf->_node;
  return eolTrue;
}

eolBool eol_config_get_keychain_by_tag(eolKeychain **output,
                                       eolConfig *conf,
                                       eolLine tag)
{
  g_return_val_if_fail(conf->_node, eolFalse);
  g_return_val_if_fail(output, eolFalse);
  *output = eol_keychain_get_hash_value(conf->_node, tag);
  if (*output == NULL)return eolFalse;
  return eolTrue;
}


eolBool eol_config_get_vec3d_by_tag(
  eolVec3D  *output,
  eolConfig *conf,
  eolLine    tag
)
{
  g_return_val_if_fail(conf, eolFalse);
  g_return_val_if_fail(output, eolFalse);
  return eol_keychain_get_hash_value_as_vec3d(output, conf->_node, tag);
}

eolBool eol_config_get_float_by_tag(eolFloat *output, eolConfig *conf, eolLine tag)
{
  g_return_val_if_fail(conf, eolFalse);
  g_return_val_if_fail(output, eolFalse);
  return eol_keychain_get_hash_value_as_float(output, conf->_node, tag);
}

eolBool eol_config_get_int_by_tag(eolInt *output, eolConfig *conf, eolLine tag)
{
  g_return_val_if_fail(conf, eolFalse);
  g_return_val_if_fail(output, eolFalse);
  return eol_keychain_get_hash_value_as_int(output, conf->_node, tag);
}

eolBool eol_config_get_uint_by_tag(eolUint *output, eolConfig *conf, eolLine tag)
{
  g_return_val_if_fail(conf, eolFalse);
  g_return_val_if_fail(output, eolFalse);
  return eol_keychain_get_hash_value_as_uint(output, conf->_node, tag);
}

eolBool eol_config_get_bool_by_tag(eolBool *output, eolConfig *conf, eolLine tag)
{
  g_return_val_if_fail(conf, eolFalse);
  g_return_val_if_fail(output, eolFalse);
  return eol_keychain_get_hash_value_as_bool(output, conf->_node, tag);
}

eolBool eol_config_get_line_by_tag( eolLine output,  eolConfig *conf, eolLine tag)
{
  g_return_val_if_fail(conf, eolFalse);
  return eol_keychain_get_hash_value_as_line(output, conf->_node, tag);
}

eolBool eol_config_get_rectfloat_by_tag( eolRectFloat * output,  eolConfig *conf, eolLine tag)
{
  g_return_val_if_fail(conf, eolFalse);
  return eol_keychain_get_hash_value_as_rectfloat(output, conf->_node, tag);
}


/*eol@eof*/

