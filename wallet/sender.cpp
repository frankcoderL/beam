#include "sender.h"

namespace
{
    void GenerateRandom(void* p, uint32_t n)
    {
        for (uint32_t i = 0; i < n; i++)
            ((uint8_t*) p)[i] = (uint8_t) rand();
    }

    void SetRandom(ECC::uintBig& x)
    {
        GenerateRandom(x.m_pData, sizeof(x.m_pData));
    }

    void SetRandom(ECC::Scalar::Native& x)
    {
        ECC::Scalar s;
        while (true)
        {
            SetRandom(s.m_Value);
            if (!x.Import(s))
                break;
        }
    }
}

namespace beam::wallet
{
    void Sender::FSMDefinition::initTx(const msmf::none&)
    {
        auto invitationData = std::make_shared<sender::InvitationData>();
        // 1. Create transaction Uuid
        invitationData->m_txId = m_txId;

        auto coins = m_keychain->getCoins(m_amount); // need to lock 
        invitationData->m_amount = m_amount;
        m_kernel.get_Hash(invitationData->m_message);
        
        // 2. Set lock_height for output (current chain height)
        // 3. Select inputs using desired selection strategy
        {
            m_blindingExcess = ECC::Zero;
            for (const auto& coin: coins)
            {
                Input::Ptr input = std::make_unique<Input>();

                ECC::Scalar::Native key(coin.m_key);
                input->m_Commitment = ECC::Commitment(key, coin.m_amount);

                invitationData->m_inputs.push_back(std::move(input));
                
                m_blindingExcess += key;
            }
        }
        // 4. Create change_output
        // 5. Select blinding factor for change_output
        {
            Amount change = 0;
            for (const auto &coin : coins)
            {
                change += coin.m_amount;
            }

            change -= m_amount;

            Output::Ptr output = std::make_unique<Output>();

            ECC::Scalar::Native blindingFactor;
            SetRandom(blindingFactor);
            output->m_Commitment = ECC::Commitment(blindingFactor, change);

            output->m_pPublic.reset(new ECC::RangeProof::Public);
            output->m_pPublic->m_Value = change;
            output->m_pPublic->Create(blindingFactor);
            // TODO: need to store new key and amount in keyChain

            blindingFactor = -blindingFactor;
            m_blindingExcess += blindingFactor;

            invitationData->m_outputs.push_back(std::move(output));
        }
        // 6. calculate tx_weight
        // 7. calculate fee
        // 8. Calculate total blinding excess for all inputs and outputs xS
        // 9. Select random nonce kS
        ECC::Signature::MultiSig msig;
        SetRandom(m_nonce);

        msig.m_Nonce = m_nonce;
        // 10. Multiply xS and kS by generator G to create public curve points xSG and kSG
        m_publicBlindingExcess 
            = invitationData->m_publicSenderBlindingExcess
            = ECC::Context::get().G * m_blindingExcess;
        m_publicNonce 
            = invitationData->m_publicSenderNonce
            = ECC::Context::get().G * m_nonce;
        // an attempt to implement "stingy" transaction
        m_gateway.sendTxInitiation(invitationData);
    }

    bool Sender::FSMDefinition::isValidSignature(const TxInitCompleted& event)
    {
        auto data = event.data;
        // 4. Compute Sender Schnorr signature
        ECC::Signature::MultiSig msig;
        msig.m_Nonce = m_nonce;
        msig.m_NoncePub = m_publicNonce + data->m_publicReceiverNonce;
        ECC::Hash::Value message;
        m_kernel.get_Hash(message);
        m_kernel.m_Signature.CoSign(m_senderSignature, message, m_blindingExcess, msig);
        // 1. Calculate message m

        // 2. Compute Schnorr challenge e
        ECC::Point::Native k;
        k = m_publicNonce + data->m_publicReceiverNonce;
        ECC::Scalar::Native e = m_kernel.m_Signature.m_e;
 
        // 3. Verify recepients Schnorr signature 
        ECC::Point::Native s, s2;
        ECC::Scalar::Native ne;
        ne = -e;
        s = data->m_publicReceiverNonce;
        s += data->m_publicReceiverBlindingExcess * ne;

        s2 = ECC::Context::get().G * data->m_receiverSignature;
        ECC::Point p(s), p2(s2);

        return (p == p2);
    }

    bool Sender::FSMDefinition::isInvalidSignature(const TxInitCompleted& event)
    {
        return !isValidSignature(event);
    }

    void Sender::FSMDefinition::confirmTx(const TxInitCompleted& event)
    {
        auto data = event.data;
        // 4. Compute Sender Schnorr signature
        auto confirmationData = std::make_shared<sender::ConfirmationData>();
        confirmationData->m_txId = m_txId;
        ECC::Signature::MultiSig msig;
        msig.m_Nonce = m_nonce;
        msig.m_NoncePub = m_publicNonce + data->m_publicReceiverNonce;
        ECC::Hash::Value message;
        m_kernel.get_Hash(message);
        m_kernel.m_Signature.CoSign(confirmationData->m_senderSignature, message, m_blindingExcess, msig);
        m_gateway.sendTxConfirmation(confirmationData);
    }

    void Sender::FSMDefinition::rollbackTx(const TxFailed& )
    {

    }

    void Sender::FSMDefinition::cancelTx(const TxInitCompleted& )
    {
        
    }

    void Sender::FSMDefinition::confirmChangeOutput(const TxConfirmationCompleted&)
    {
        m_gateway.sendChangeOutputConfirmation();
    }

    void Sender::FSMDefinition::completeTx(const TxOutputConfirmCompleted&)
    {
        std::cout << "Sender::completeTx\n";
    }
}
