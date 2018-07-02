#pragma once

#include "core/common.h"
#include "core/ecc_native.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4127 )
#endif

#include <boost/msm/back/state_machine.hpp>
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/msm/front/functor_row.hpp>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "core/serialization_adapters.h"

namespace beam
{
    using Uuid = std::array<uint8_t, 16>;

    struct PrintableAmount
    {
        explicit PrintableAmount(const Amount& amount, bool showPoint = false) : m_value{ amount }, m_showPoint{showPoint}
        {}
        const Amount& m_value;
        bool m_showPoint;
    };

    std::ostream& operator<<(std::ostream& os, const PrintableAmount& amount);
    std::ostream& operator<<(std::ostream& os, const Uuid& uuid);

    struct Coin;
    using TransactionPtr = std::shared_ptr<Transaction>;
    
    struct TxDescription
    {
        enum Status
        {
            Pending,
            InProgress,
            Cancelled,
            Completed,
            Failed
        };

        TxDescription() = default;

        TxDescription(const Uuid& txId
            , Amount amount
            , Amount fee
            , uint64_t peerId
            , ByteBuffer&& message
            , Timestamp createTime
            , bool sender)
            : m_txId{ txId }
            , m_amount{ amount }
            , m_fee{ fee }
            , m_peerId{ peerId }
            , m_message{ std::move(message) }
            , m_createTime{ createTime }
            , m_modifyTime{ createTime }
            , m_sender{ sender }
            , m_status{ Pending }
            , m_fsmState{}
        {}

        Uuid m_txId;
        Amount m_amount;
        Amount m_fee;
        uint64_t m_peerId;
        ByteBuffer m_message;
        Timestamp m_createTime;
        Timestamp m_modifyTime;
        bool m_sender;
        Status m_status;
        ByteBuffer m_fsmState;
    };

    namespace wallet
    {
        namespace msm = boost::msm;
        namespace msmf = boost::msm::front;
        namespace mpl = boost::mpl;

        std::pair<ECC::Scalar::Native, ECC::Scalar::Native> splitKey(const ECC::Scalar::Native& key, uint64_t index);
        Timestamp getTimestamp();

        // messages
        struct InviteReceiver
        {
            Uuid m_txId;
            ECC::Amount m_amount;
            ECC::Amount m_fee;
            ECC::Point m_publicSenderExcess;
            ECC::Scalar m_offset;
            ECC::Point m_publicSenderNonce;
            std::vector<Input::Ptr> m_inputs;
            std::vector<Output::Ptr> m_outputs;

            InviteReceiver() : m_amount(0)
            {

            }

            InviteReceiver(InviteReceiver&& other)
                : m_txId{other.m_txId}
                , m_amount{ other.m_amount }
                , m_fee{ other.m_fee }
                , m_publicSenderExcess{other.m_publicSenderExcess}
                , m_offset{other.m_offset}
                , m_publicSenderNonce{other.m_publicSenderNonce}
                , m_inputs{std::move(other.m_inputs)}
                , m_outputs{std::move(other.m_outputs)}
            {

            }

            SERIALIZE(m_txId
                    , m_amount
                    , m_fee
                    , m_publicSenderExcess
                    , m_offset
                    , m_publicSenderNonce
                    , m_inputs
                    , m_outputs);
        };

        struct ConfirmTransaction
        {
            Uuid m_txId{};
            ECC::Scalar m_senderSignature;

            SERIALIZE(m_txId, m_senderSignature);
        };

        struct ConfirmInvitation
        {
            Uuid m_txId{};
            ECC::Point m_publicPeerBlindingExcess;
            ECC::Point m_publicPeerNonce;
            ECC::Scalar m_peerSignature;

            SERIALIZE(m_txId
                    , m_publicPeerBlindingExcess
                    , m_publicPeerNonce
                    , m_peerSignature);
        };

        struct TxRegistered
        {
            Uuid m_txId;
            bool m_value;
            SERIALIZE(m_txId, m_value);
        };

        struct TxFailed
        {
            Uuid m_txId;
            SERIALIZE(m_txId);
        };

        struct IWalletGateway
        {
            virtual ~IWalletGateway() {}
            virtual void on_tx_completed(const TxDescription& ) = 0;
            virtual void send_tx_failed(const TxDescription& ) = 0;
        };

        namespace sender
        {
            struct IGateway : virtual IWalletGateway
            {
                virtual void send_tx_invitation(const TxDescription&, InviteReceiver&&) = 0;
                virtual void send_tx_confirmation(const TxDescription& , ConfirmTransaction&&) = 0;
                virtual void send_tx_confirmation(const TxDescription&, ConfirmInvitation&&) = 0;
                virtual void register_tx(const TxDescription&, Transaction::Ptr) = 0;
                virtual void send_tx_registered(const TxDescription&) = 0;
            };
        }

        namespace receiver
        {
            struct IGateway : virtual IWalletGateway
            {
                virtual void send_tx_confirmation(const TxDescription& , ConfirmInvitation&&) = 0;
                virtual void register_tx(const TxDescription& , Transaction::Ptr) = 0;
                virtual void send_tx_registered(const TxDescription& ) = 0;
            };
        }
    }
}
