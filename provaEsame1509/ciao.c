for(int i = 0; i < strlen(str); i++){
            if(str[i] == ':'){
                num_char = i;
                break;
            }
        }

        strncpy(database[j].nome, str, num_char);

        strncpy(val, &str[num_char+1], strlen(str)-1 - num_char);

        database[j].valore = atoi(val);