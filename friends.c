#include "friends.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Create a new user with the given name.  Insert it at the tail of the list
 * of users whose head is pointed to by *user_ptr_add.
 *
 * Return:
 *   - 0 on success.
 *   - 1 if a user by this name already exists in this list.
 *   - 2 if the given name cannot fit in the 'name' array
 *       (don't forget about the null terminator).
 */
int create_user(const char *name, User **user_ptr_add) {
    if (strlen(name) >= MAX_NAME) {
        return 2;
    }

    User *new_user = malloc(sizeof(User));
    if (new_user == NULL) {
        perror("malloc");
        exit(1);
    }
    strncpy(new_user->name, name, MAX_NAME); // name has max length MAX_NAME - 1

    for (int i = 0; i < MAX_NAME; i++) {
        new_user->profile_pic[i] = '\0';
    }

    new_user->first_post = NULL;
    new_user->next = NULL;
    for (int i = 0; i < MAX_FRIENDS; i++) {
        new_user->friends[i] = NULL;
    }

    // Add user to list
    User *prev = NULL;
    User *curr = *user_ptr_add;
    while (curr != NULL && strcmp(curr->name, name) != 0) {
        prev = curr;
        curr = curr->next;
    }

    if (*user_ptr_add == NULL) {
        *user_ptr_add = new_user;
        return 0;
    } else if (curr != NULL) {
        free(new_user);
        return 1;
    } else {
        prev->next = new_user;
        return 0;
    }
}


/*
 * Return a pointer to the user with this name in
 * the list starting with head. Return NULL if no such user exists.
 *
 * NOTE: You'll likely need to cast a (const User *) to a (User *)
 * to satisfy the prototype without warnings.
 */
User *find_user(const char *name, const User *head) {
    while (head != NULL && strcmp(name, head->name) != 0) {
        head = head->next;
    }

    return (User *)head;
}


/*
 * Return a dynamically allocated string containing a list of all users
 */
char* list_users(const User *curr) {
    // First loop until curr is NULL to find the total number of chars to allocate
    char* list;
    int list_len = 0;
    const User *head = curr;
    while (curr != NULL) {
        list_len += strlen(curr->name) + 1;    // +2 for tab
        curr = curr->next;
        if (curr != NULL) {
            list_len += 2;  // +2 for \r\n
        }
    }
    list_len += 1;   // +1 for null terminator
    list_len += 11;  // +11 for "User List\r\n"

    list_len += 2;    // for \r\n;
    list = malloc(list_len * sizeof(char));
    strcpy(list, "");
    if (list == NULL) {
        perror("friends.c: malloc");
        exit(1);
    }
    else{
        curr = head;
        char u[] = "User List\r\n";
        strcat(list, u);
        while (curr != NULL) {
            // start concatnating to the list each name
            strcat(list, "\t");
            strcat(list, curr->name);
            curr = curr->next;
            if (curr != NULL) {
                strcat(list, "\r\n");
            }
        }
    }
    strcat(list, "\r");
    strcat(list, "\n");
    list[list_len - 1] = '\0';
    return list;
    
}


/*
 * Make two users friends with each other.  This is symmetric - a pointer to
 * each user must be stored in the 'friends' array of the other.
 *
 * New friends must be added in the first empty spot in the 'friends' array.
 *
 * Return:
 *   - 0 on success.
 *   - 1 if the two users are already friends.
 *   - 2 if the users are not already friends, but at least one already has
 *     MAX_FRIENDS friends.
 *   - 3 if the same user is passed in twice.
 *   - 4 if at least one user does not exist.
 *
 * Do not modify either user if the result is a failure.
 * NOTE: If multiple errors apply, return the *largest* error code that applies.
 */
int make_friends(const char *name1, const char *name2, User *head) {
    User *user1 = find_user(name1, head);
    User *user2 = find_user(name2, head);

    if (user1 == NULL || user2 == NULL) {
        return 4;
    } else if (user1 == user2) { // Same user
        return 3;
    }

    int i, j;
    for (i = 0; i < MAX_FRIENDS; i++) {
        if (user1->friends[i] == NULL) { // Empty spot
            break;
        } else if (user1->friends[i] == user2) { // Already friends.
            return 1;
        }
    }

    for (j = 0; j < MAX_FRIENDS; j++) {
        if (user2->friends[j] == NULL) { // Empty spot
            break;
        }
    }

    if (i == MAX_FRIENDS || j == MAX_FRIENDS) { // Too many friends.
        return 2;
    }

    user1->friends[i] = user2;
    user2->friends[j] = user1;
    return 0;
}


/*
 *  Dynamically allocate a string containing contents 
 *  that original print_post would print
 *  Return an empty string if post is NULL
 */
 
char* print_post(const Post *post) {
    if (post == NULL) {
        char* empty = malloc(sizeof(char));
        empty[0] = '\0';
        return empty;
    }
    
    // First calculate total number of characters to allocate
    int result_len = 0;
    result_len += 6;     // for "From: "
    result_len += strlen(post->author);
    result_len += 2;     // for \r\n
    result_len += 6;     // for "Date: "
    char *date = asctime(localtime(post->date));
    result_len += strlen(date);
    result_len += 2;     // for \r\n
    result_len += strlen(post->contents);
    result_len += 2;     // for \r\n

    char* result = malloc(result_len * sizeof(char) + 1);
    strcpy(result, "");
    if (result == NULL) {
        perror("friends.c: malloc");
        exit(1);
    }
    
    // Start concatenating to result
    char f[] = "From: ";
    strcat(result, f);
    strcat(result, post->author);
    strcat(result, "\r");
    strcat(result, "\n");

    char d[] = "Date: ";
    strcat(result, d);
    strcat(result, date);
    strcat(result, "\r");
    strcat(result, "\n");

    strcat(result, post->contents);
    strcat(result, "\r");
    strcat(result, "\n");

    result[result_len] = '\0';
    return result;
}


/*
 *  Return dynamically allocated string containing the contents original print_user
 *  Return an empty string if user is NULL
 */
char* print_user(const User *user) {
    /* if (user == NULL) {
        return 1;
    }
    */
    if (user == NULL) {
        char* empty = malloc(sizeof(char));
        empty[0] = '\0';
        return empty;
    }

    // Use snprintf to first calculate the total number of characters
    // we need to allocate for the string
    int result_len = 0;
    char lines[] = "------------------------------------------\r\n";
    result_len += 6;     // for "Name: "
    result_len += strlen(user->name);
    result_len += 2;     // for \r\n 
    result_len += strlen(lines);  // for lines

    result_len += 9;     // for "Friends:\r\n"
    
    // loop through friends to find the total number of characters
    for (int i = 0; i < MAX_FRIENDS && user->friends[i] != NULL; i++) {
        result_len += strlen(user->friends[i]->name);
        result_len += 2;  // for \r\n
    }
    result_len += strlen(lines);  // for lines
    result_len += 8;     // for "Posts:\r\n"
    const Post *curr = user->first_post;
    // loop through posts to find the total number of characters
    while (curr != NULL) {
        char *printed_post = print_post(curr);
        result_len += strlen(printed_post);
        free(printed_post);
        curr = curr->next;
        if (curr != NULL) {
            result_len += 7;  // for "\r\n===\r\n"
        }
    }

    result_len += 2;    // for \r\n;
    result_len += strlen(lines);  // for lines

    char* result = malloc(result_len * sizeof(char) + 1);
    strcpy(result, "");
    if (result == NULL) {
        perror("friends.c: malloc");
        exit(1);
    }

    // Start concatenating to result
    char n[] = "Name: ";
    strcat(result, n);
    strcat(result, user->name);
    strcat(result, "\r\n");
    strcat(result, lines);

    char f[] = "Friends:\r\n";
    strcat(result, f);
    // Loop through friends to add to result
    for (int i = 0; i < MAX_FRIENDS && user->friends[i] != NULL; i++) {
        strcat(result, user->friends[i]->name);
        strcat(result, "\r");
        strcat(result, "\n");
    }
    strcat(result, lines);

    char p[] = "Posts:\r\n";
    strcat(result, p);
    // Loop through posts to add to result
    curr = user->first_post;
    while (curr != NULL) {
        char *printed_post = print_post(curr);
        strcat(result, printed_post);
        free(printed_post);
        curr = curr->next;
        if (curr != NULL) {
            strcat(result, "\r\n===\r\n");
        }
    }
    strcat(result, lines);

    strcat(result, "\r");
    strcat(result, "\n");
    result[result_len] = '\0';
    return result;
    
}


/*
 * Make a new post from 'author' to the 'target' user,
 * containing the given contents, IF the users are friends.
 *
 * Insert the new post at the *front* of the user's list of posts.
 *
 * Use the 'time' function to store the current time.
 *
 * 'contents' is a pointer to heap-allocated memory - you do not need
 * to allocate more memory to store the contents of the post.
 *
 * Return:
 *   - 0 on success
 *   - 1 if users exist but are not friends
 *   - 2 if either User pointer is NULL
 */
int make_post(const User *author, User *target, char *contents) {
    if (target == NULL || author == NULL) {
        return 2;
    }

    int friends = 0;
    for (int i = 0; i < MAX_FRIENDS && target->friends[i] != NULL; i++) {
        if (strcmp(target->friends[i]->name, author->name) == 0) {
            friends = 1;
            break;
        }
    }

    if (friends == 0) {
        return 1;
    }

    // Create post
    Post *new_post = malloc(sizeof(Post));
    if (new_post == NULL) {
        perror("malloc");
        exit(1);
    }
    strncpy(new_post->author, author->name, MAX_NAME);
    new_post->contents = contents;
    new_post->date = malloc(sizeof(time_t));
    if (new_post->date == NULL) {
        perror("malloc");
        exit(1);
    }
    time(new_post->date);
    new_post->next = target->first_post;
    target->first_post = new_post;

    return 0;
}

