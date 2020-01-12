import React from 'react';

import { useTranslation } from 'react-i18next';

import { Icon } from '@blueprintjs/core';
import { IconNames } from '@blueprintjs/icons';

import styles from './EditorOutlineSidebar.module.scss';

export interface EditorOutlineHeaderProps {
  onCloseClicked: () => void;
}

export const EditorOutlineHeader: React.FC<EditorOutlineHeaderProps> = props => {
  const { t } = useTranslation();

  return (
    <div className={styles.outlineHeader}>
      <Icon icon={IconNames.ALIGN_JUSTIFY} />
      <div className={styles.outlineHeaderText}>{t('outline_header_text')}</div>
      <Icon
        icon={IconNames.CHEVRON_LEFT}
        htmlTitle={t('close_button_title')}
        className={styles.outlineCloseIcon}
        onClick={props.onCloseClicked}
      />
    </div>
  );
};