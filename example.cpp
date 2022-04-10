#include <publisher.hpp>

#include <iostream>


struct UserCreated {
    std::string username;
    std::string email;
    /* something other */
};

struct UserRemoved {
    std::string username;
    std::string email;
};

struct UserWonLottery {
    std::string username;
    std::string email;
    std::uint64_t prize_id;
};


struct EmailNotifier {
    void on_user_created(UserCreated& event) {
        std::cout << "Hello, " << event.username << ", thanks for signing up!" << std::endl;
    }

    void on_user_removed(UserRemoved& event) {
        std::cout << "Bye, " << event.username << "!" << std::endl;
    }

    void on_user_won_lottery(UserWonLottery& event) {
        std::cout << "Hello, " << event.username << ", you won lottery!" << std::endl;
    }
};


struct LotteryService : Publisher {

    void on_user_created(UserCreated & event) {
        if (true) {
            publish(UserWonLottery{std::move(event.username), std::move(event.email), 33133});
        }
    }
};


struct User {
    std::uint64_t id;
    std::string username;
    std::string password;
    std::string email;
};

class UserService : public Publisher {

public:
    std::uint64_t create_user(std::string username, std::string password, std::string email) {
        auto generated_id = ++fake_id_generator_;

        auto user = User
        {
            generated_id,
            std::move(username),
            std::move(password),
            std::move(email)
        };

        database_[generated_id] = std::move(user);

        publish(UserCreated{ database_[generated_id].username, database_[generated_id].email });

        return generated_id;
    }

    void remove_user(std::uint64_t id) {
        auto username = std::move(database_[id].username);
        auto email = std::move(database_[id].email);

        publish(UserRemoved{std::move(username), std::move(email)});
    }
private:
    std::unordered_map<std::uint64_t, User> database_;
    std::uint64_t fake_id_generator_ = { 0 };
};

 
int main() {

    UserService user_service;
    EmailNotifier notifier;
    LotteryService lottery_service;


    user_service.subscribe<UserCreated>(
        [&notifier](UserCreated& event) { notifier.on_user_created(event); }
    );

    user_service.subscribe<UserCreated>(
        [&lottery_service](UserCreated& event) { lottery_service.on_user_created(event); }
    );

    /* keep token if you want to remove handler later */
    auto lottery_token = lottery_service.subscribe<UserWonLottery>(
        [&notifier](UserWonLottery& event) { notifier.on_user_won_lottery(event); }
    );

    user_service.subscribe<UserRemoved>(
        [&notifier](UserRemoved& event) { notifier.on_user_removed(event); }
    );



    auto max_id = user_service.create_user("maxim", "pass", "max@some-mail.com");
    std::cout << std::endl;

    /* no more lottery for new users */
    lottery_service.unsubscribe(lottery_token);
    user_service.create_user("john", "password", "john@some-mail.com");
    std::cout << std::endl;

    user_service.remove_user(max_id);
}
