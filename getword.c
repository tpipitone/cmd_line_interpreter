// program 1 remake 
//

#include "getword.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int getword(char *w){
	int len;
	char c; 
//	if(strcmp(w, "end.") == 0) {return -1;} 
	*w = '\0'; //init
	len = 0;
	c = '\0'; 	

	c = getchar(); 
	while(c == ' '){c = getchar();} // discards leading spaces and slashes 
	
	if(c == '\n'){ return 0;} //returns 0 if newline, either due to ungetc() or a starting newline 
	if(c == EOF){return -1;}
 	if(c == '\\'){
		c = getchar(); 
		if(c == '|'){
			*w++ = '|';
			*w++ = ' ';
			len += 2;  
			c = getchar();
			while(c == ' ') {c = getchar(); }	
		}
	}
	
	if(c == '>'){
		c = getchar(); 
		if(c == '>'){ // this means we have at least '>>', still have the possibility for >>& 	
			c = getchar(); 
			if(c == '&') { // this means we have '>>&'
				*w++ = '>';
				*w++ = '>'; 
				*w++ = '&'; 
				*w++ = '\0'; 
				return 3; 
			} 
			c = ungetc(c, stdin); 
			*w++ = '>';
			*w++ = '>'; 
			*w++ = '\0';
			return 2; 
		} else if (c == '&') { // this means we have '>&' 
		 	*w++ = '>';
			*w++ = '&';
			*w++ = '\0';
			return 2; 
			
		} else { // only '>' found
			c = ungetc(c, stdin); 
			*w++ = '>'; 
			*w++ = '\0'; 
			return 1; 
		}



	}	
	
	if(c == '<' || c == '|' || c == '&' ){
		*w++ = c; 
		*w++ = '\0'; 
		return 1; 
	}
	
	for(;;){		
		if(len == STORAGE-1){
			c = ungetc(c, stdin); 
			break;
		}
	 
		if(c == '\\'){
			c = getchar(); // get char after;
			if(c == '\n'){
				c = ungetc(c,stdin) ;
				*w++='\0';
				break;}
			
			if(c == ' '){
				break; 
			}
			
			len++; 
			*w++ = c; // add to arr, no speacial treatment
			c = getchar(); // grab next char to be treated normally	
		}
		


		if(c == ' '){
			*w++ = '\0';
			break;
		}
	
		if(c == '\n' || c == EOF){
			c = ungetc(c,stdin); // puts newline back and breaks, to be handled by next call of function
			*w++ = '\0';
			break;
		}
		
		if( (c == '>' || c == '<'|| c == '&' || c == '|') ){
			c = ungetc(c,stdin);
			*w++ = '\0' ;
			break; 
		}  

		len++;	
		*w++ = c; 
		c = getchar(); 
	}
	//if(strcmp(w, "end.")==0){return -1;}
	 
	return len; 
}
